#pragma once
#include "api/MediaTypes.h"
namespace player
{
    class IVideoSink
    {
    public:
        virtual ~IVideoSink() = default;
        virtual void submit(VideoFrame) = 0;
        virtual void flush() = 0;
    };
}
