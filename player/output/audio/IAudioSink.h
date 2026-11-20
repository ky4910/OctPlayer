#pragma once
#include "api/MediaTypes.h"
#include <span>
namespace player
{
    class IAudioSink
    {
    public:
        virtual ~IAudioSink() = default;
        virtual bool open(const AudioFormat &) = 0;
        virtual void start() = 0;
        virtual void pause() = 0;
        virtual void stop() = 0;
        virtual void flush() = 0;
        virtual void close() = 0;
        virtual bool write(std::span<const std::byte>) = 0;
        virtual MediaTime playedTime() const = 0;
        virtual MediaTime queuedTime() const = 0;
    };
}
