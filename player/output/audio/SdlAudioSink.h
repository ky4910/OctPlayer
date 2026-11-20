#pragma once
#include "output/audio/IAudioSink.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <span>
namespace player
{
    class SdlAudioSink final : public IAudioSink
    {
    public:
        SdlAudioSink();
        ~SdlAudioSink() override;
        bool open(const AudioFormat &) override;
        void start() override;
        void pause() override;
        void stop() override;
        void flush() override;
        void close() override;
        bool write(std::span<const std::byte>) override;
        MediaTime playedTime() const override;
        MediaTime queuedTime() const override;

    private:
        mutable std::mutex mutex_;
        SDL_AudioStream *stream_{};
        AudioFormat format_{};
        std::atomic<std::uint64_t> submittedBytes_{};
    };
}
