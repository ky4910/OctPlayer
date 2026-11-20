#pragma once
#include "api/IPlayer.h"
#include "core/PlaybackSession.h"
#include "output/audio/SdlAudioSink.h"
#include "output/video/VideoFrameMailbox.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <variant>

namespace player
{
    class Player final : public IPlayer
    {
    public:
        Player();
        ~Player() override;
        void open(MediaSource) override;
        void play() override;
        void resume() override;
        void pause() override;
        void stop() override;
        void seek(MediaTime) override;
        void setVolume(float) override;
        void setMuted(bool) override;
        void setRate(double) override;
        PlayerSnapshot snapshot() const override;
        void setEventCallback(PlayerEventCallback) override;
        std::shared_ptr<VideoFrameMailbox> videoMailbox() const { return video_; }

    private:
        struct OpenCommand
        {
            MediaSource source;
        };
        struct PlayCommand
        {
        };
        struct ResumeCommand
        {
        };
        struct PauseCommand
        {
        };
        struct StopCommand
        {
        };
        struct SeekCommand
        {
            MediaTime position;
        };
        struct SetVolumeCommand
        {
            float value;
        };
        struct SetMutedCommand
        {
            bool value;
        };
        struct SetRateCommand
        {
            double value;
        };

        using Command = std::variant<
            OpenCommand,
            PlayCommand,
            ResumeCommand,
            PauseCommand,
            StopCommand,
            SeekCommand,
            SetVolumeCommand,
            SetMutedCommand,
            SetRateCommand>;

        template <class F>
        void withSession(F &&f)
        {
            std::scoped_lock l(m_);
            if (session_)
                f(*session_);
        }

        void runControlLoop();
        void dispatch(Command command);
        void processCommand(const Command &command);

        mutable std::mutex m_;
        std::thread controlThread_;
        std::mutex commandMutex_;
        std::condition_variable commandCv_;
        std::deque<Command> commandQueue_;
        bool quit_{false};
        std::atomic<std::uint64_t> activeSessionId_{0};
        std::atomic<std::uint64_t> activeGeneration_{0};
        std::uint64_t nextSession_{1};
        std::uint64_t nextGeneration_{1};
        std::unique_ptr<PlaybackSession> session_;
        std::shared_ptr<SdlAudioSink> audio_;
        std::shared_ptr<VideoFrameMailbox> video_;
        PlayerEventCallback event_;
        PlayerSnapshot fallback_;
    };
}
