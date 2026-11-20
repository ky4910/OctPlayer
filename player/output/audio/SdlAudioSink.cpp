#include "output/audio/SdlAudioSink.h"

#include <QDebug>
#include <algorithm>

namespace player
{
    SdlAudioSink::SdlAudioSink() { SDL_InitSubSystem(SDL_INIT_AUDIO); }
    SdlAudioSink::~SdlAudioSink()
    {
        close();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    bool SdlAudioSink::open(const AudioFormat &f)
    {
        close();
        std::lock_guard<std::mutex> lock(mutex_);
        format_ = f;
        submittedBytes_ = 0;

        SDL_AudioSpec want{};
        want.freq = f.sampleRate;
        want.channels = static_cast<Uint8>(f.channels);
        want.format = SDL_AUDIO_S16;

        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, nullptr, nullptr);

        if (!stream_)
            return false;

        // Keep the device paused until PlaybackSession::play() starts playback.
        // Opening a stream must not implicitly start audio during the Open phase.
        return true;
    }

    void SdlAudioSink::start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_)
        {
            SDL_ResumeAudioStreamDevice(stream_);
        }
        // TODO: There should be any handle logic? eg: else case?
    }

    void SdlAudioSink::pause()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_)
        {
            SDL_PauseAudioStreamDevice(stream_);
        }
        // TODO: There should be any handle logic? eg: else case?
    }

    void SdlAudioSink::stop()
    {
        SDL_AudioStream *stream = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stream = stream_;
            if (stream)
                SDL_PauseAudioStreamDevice(stream);
            if (stream)
                SDL_ClearAudioStream(stream);
            submittedBytes_ = 0;
            stream_ = nullptr;
        }
        if (stream)
            SDL_DestroyAudioStream(stream);
    }

    void SdlAudioSink::flush()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_)
            SDL_ClearAudioStream(stream_);
        submittedBytes_ = 0;
    }

    void SdlAudioSink::close()
    {
        SDL_AudioStream *stream = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stream = stream_;
            stream_ = nullptr;
        }
        if (stream)
            SDL_DestroyAudioStream(stream);
    }

    bool SdlAudioSink::write(std::span<const std::byte> b)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stream_)
        {
            return false;
        }
        if (b.empty())
            return true;

        const bool ok = SDL_PutAudioStreamData(stream_, b.data(), static_cast<int>(b.size()));
        if (!ok)
        {
            qDebug() << "SdlAudioSink write failed stream=" << static_cast<void *>(stream_)
                     << "bytes=" << b.size() << "SDL=" << SDL_GetError();
            return false;
        }

        submittedBytes_ += b.size();
        return true;
    }

    MediaTime SdlAudioSink::playedTime() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stream_)
            return MediaTime{};

        const auto submitted = submittedBytes_.load();
        const auto queued = static_cast<std::uint64_t>(std::max(SDL_GetAudioStreamQueued(stream_), 0));
        auto played = submitted > queued ? submitted - queued : 0;
        auto bps = std::uint64_t(format_.sampleRate) * format_.channels * format_.bytesPerSample;

        const MediaTime result = bps ? MediaTime(played * 1000000 / bps) : MediaTime{};
        return result;
    }

    MediaTime SdlAudioSink::queuedTime() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stream_)
            return MediaTime{};

        const auto queued = static_cast<std::uint64_t>(std::max(SDL_GetAudioStreamQueued(stream_), 0));
        const auto bytesPerSecond = std::uint64_t(format_.sampleRate) * format_.channels * format_.bytesPerSample;
        const MediaTime result = bytesPerSecond ? MediaTime(queued * 1000000 / bytesPerSecond) : MediaTime{};

        static std::uint64_t queuedLogCount = 0;
        return result;
    }
}
