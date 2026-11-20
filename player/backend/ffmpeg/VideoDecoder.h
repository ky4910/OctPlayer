#pragma once

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "backend/ffmpeg/FfmpegRaii.h"

#include <string>

namespace player
{
    class VideoDecoder
    {
    public:
        VideoDecoder() = default;
        ~VideoDecoder();

        bool open(const AVCodecParameters *params, std::string &error);
        bool sendPacket(const AVPacket *packet, std::string &error);
        bool receiveFrame(FramePtr &frame, std::string &error);
        void flush();
        void drain();
        void close();
        bool isOpen() const { return codec_ != nullptr; }
        AVCodecContext *context() const { return codec_.get(); }

    private:
        static std::string fferr(int code);

        CodecPtr codec_;
    };
}
