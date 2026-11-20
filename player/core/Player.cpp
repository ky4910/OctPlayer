#include "core/Player.h"

#include <QDebug>

namespace player
{
    Player::Player()
        :   audio_(std::make_shared<SdlAudioSink>()),
            video_(std::make_shared<VideoFrameMailbox>())
    {
        controlThread_ = std::thread(&Player::runControlLoop, this);
    }

    Player::~Player()
    {
        {
            std::lock_guard<std::mutex> lock(commandMutex_);
            quit_ = true;
        }
        commandCv_.notify_all();

        if (controlThread_.joinable())
            controlThread_.join();

        // The control loop may have drained a queued Play command immediately
        // before shutdown. Stop the remaining session so its worker threads are
        // joined before PlaybackSession's std::thread members are destroyed.
        std::unique_ptr<PlaybackSession> session;
        {
            std::scoped_lock lock(m_);
            session = std::move(session_);
        }
        if (session)
            session->stop();
    }

    void Player::runControlLoop()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock(commandMutex_);
            commandCv_.wait(lock, [this]
                            { return quit_ || !commandQueue_.empty(); });

            if (quit_ && commandQueue_.empty())
                break;

            auto command = std::move(commandQueue_.front());
            commandQueue_.pop_front();
            lock.unlock();

            processCommand(command);
        }
    }

    void Player::dispatch(Command command)
    {
        {
            std::lock_guard<std::mutex> lock(commandMutex_);
            commandQueue_.push_back(std::move(command));
        }
        commandCv_.notify_one();
    }

    void Player::processCommand(const Command &command)
    {
        if (const auto *openCommand = std::get_if<OpenCommand>(&command))
        {
            const auto newSessionId = nextSession_++;
            const auto newGeneration = nextGeneration_++;

            activeSessionId_.store(newSessionId, std::memory_order_release);
            activeGeneration_.store(newGeneration, std::memory_order_release);

            std::unique_ptr<PlaybackSession> old;
            {
                std::scoped_lock l(m_);
                old = std::move(session_);
            }
            if (old)
            {
                old->stop();
            }

            if (audio_)
                audio_->stop();
            if (video_)
                video_->flush();

            auto fresh = std::make_unique<PlaybackSession>(newSessionId, newGeneration, audio_, video_, event_);
            const bool opened = fresh->open(openCommand->source);

            {
                std::scoped_lock l(m_);
                session_ = std::move(fresh);
            }
            activeSessionId_.store(newSessionId, std::memory_order_release);
            activeGeneration_.store(newGeneration, std::memory_order_release);
            return;
        }

        if (std::get_if<PlayCommand>(&command))
        {
            withSession([this](auto &s)
                        {
                            s.play();
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation(), std::memory_order_release);
                        });
            return;
        }

        if (std::get_if<ResumeCommand>(&command))
        {
            withSession([this](auto &s)
                        {
                            s.resume();
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation(), std::memory_order_release);
                        });
            return;
        }

        if (std::get_if<PauseCommand>(&command))
        {
            withSession([this](auto &s)
                        {
                            s.pause();
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation(), std::memory_order_release);
                        });
            return;
        }

        if (std::get_if<StopCommand>(&command))
        {
            withSession([this](auto &s)
                        {
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation() + 1, std::memory_order_release);
                            s.stop();
                            // Keep the stopped session so snapshot(), duration,
                            // and seekable remain available until the next open.
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation(), std::memory_order_release);
                        });
            return;
        }

        if (const auto *seekCommand = std::get_if<SeekCommand>(&command))
        {
            withSession([this, &seekCommand](auto &s)
                        {
                            // Publish the next generation before seek completion
                            // events are emitted, so the event filter accepts them.
                            activeGeneration_.store(s.generation() + 1, std::memory_order_release);
                            s.seek(seekCommand->position);
                            activeSessionId_.store(s.id(), std::memory_order_release);
                            activeGeneration_.store(s.generation(), std::memory_order_release);
                        });
            return;
        }

        if (const auto *volumeCommand = std::get_if<SetVolumeCommand>(&command))
        {
            std::scoped_lock l(m_);
            fallback_.volume = volumeCommand->value;
            return;
        }

        if (const auto *mutedCommand = std::get_if<SetMutedCommand>(&command))
        {
            std::scoped_lock l(m_);
            fallback_.muted = mutedCommand->value;
            return;
        }

        if (const auto *rateCommand = std::get_if<SetRateCommand>(&command))
        {
            std::scoped_lock l(m_);
            fallback_.rate = rateCommand->value;
            return;
        }
    }

    void Player::open(MediaSource s)
    {
        dispatch(OpenCommand{s});
    }

    void Player::play()
    {
        dispatch(PlayCommand{});
    }

    void Player::resume()
    {
        dispatch(ResumeCommand{});
    }

    void Player::pause()
    {
        dispatch(PauseCommand{});
    }

    void Player::stop()
    {
        dispatch(StopCommand{});
    }

    void Player::seek(MediaTime p)
    {
        dispatch(SeekCommand{p});
    }

    void Player::setVolume(float v)
    {
        dispatch(SetVolumeCommand{v});
    }

    void Player::setMuted(bool b)
    {
        dispatch(SetMutedCommand{b});
    }

    void Player::setRate(double r)
    {
        dispatch(SetRateCommand{r});
    }

    PlayerSnapshot Player::snapshot() const
    {
        std::scoped_lock l(m_);
        return session_ ? session_->snapshot() : fallback_;
    }

    void Player::setEventCallback(PlayerEventCallback c)
    {
        std::scoped_lock l(m_);

        event_ = [this, cb = std::move(c)](const PlayerEvent &e) mutable
        {
            const auto currentSessionId = activeSessionId_.load(std::memory_order_acquire);
            const auto currentGeneration = activeGeneration_.load(std::memory_order_acquire);
            if (e.sessionId != currentSessionId || e.generation != currentGeneration)
                return;

            if (cb)
                cb(e);
        };
    }

}
