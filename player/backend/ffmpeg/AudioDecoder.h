#pragma once

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "api/MediaTypes.h"
#include "backend/ffmpeg/FfmpegRaii.h"

#include <string>

namespace player
{
    class AudioDecoder
    {
    public:
        AudioDecoder() = default;
        ~AudioDecoder();

        bool open(const AVCodecParameters *params, std::string &error);
        bool sendPacket(const AVPacket *pkt, std::string &error);
        bool receiveFrame(FramePtr &frame, std::string &error);
        void flush();
        void drain();
        void close();

        AudioFormat format() const { return format_; }
        bool isOpen() const { return codec_ != nullptr; }

    private:
        static std::string fferr(int code);

        CodecPtr codec_;
        AudioFormat format_{};
    };
}
