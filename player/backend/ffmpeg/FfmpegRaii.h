#pragma once

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
}

#include <memory>

namespace player
{
    struct FormatDeleter
    {
        void operator()(AVFormatContext *p) const
        {
            if (p)
                avformat_close_input(&p);
        }
    };

    struct CodecDeleter
    {
        void operator()(AVCodecContext *p) const { avcodec_free_context(&p); }
    };

    struct FrameDeleter
    {
        void operator()(AVFrame *p) const { av_frame_free(&p); }
    };

    struct PacketDeleter
    {
        void operator()(AVPacket *p) const { av_packet_free(&p); }
    };

    using FormatPtr = std::unique_ptr<AVFormatContext, FormatDeleter>;
    using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
    using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
    using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

}
