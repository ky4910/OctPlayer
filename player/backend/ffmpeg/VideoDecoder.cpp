#include "backend/ffmpeg/VideoDecoder.h"

extern "C"
{
    #include <libavutil/error.h>
}

namespace player
{
    std::string VideoDecoder::fferr(int code)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(code, buffer, sizeof buffer);
        return buffer;
    }

    VideoDecoder::~VideoDecoder()
    {
        close();
    }

    bool VideoDecoder::open(const AVCodecParameters *params, std::string &error)
    {
        close();
        if (!params)
        {
            error = "video codec parameters are null";
            return false;
        }

        const AVCodec *codec = avcodec_find_decoder(params->codec_id);
        if (!codec)
        {
            error = "avcodec_find_decoder failed for video stream";
            return false;
        }

        CodecPtr context(avcodec_alloc_context3(codec));
        if (!context)
        {
            error = "avcodec_alloc_context3 failed for video stream";
            return false;
        }

        int result = avcodec_parameters_to_context(context.get(), params);
        if (result < 0)
        {
            error = fferr(result);
            return false;
        }

        result = avcodec_open2(context.get(), codec, nullptr);
        if (result < 0)
        {
            error = fferr(result);
            return false;
        }

        codec_ = std::move(context);
        return true;
    }

    bool VideoDecoder::sendPacket(const AVPacket *packet, std::string &error)
    {
        if (!codec_)
        {
            error = "video decoder is not opened";
            return false;
        }

        const int result = avcodec_send_packet(codec_.get(), packet);
        if (result < 0)
        {
            error = fferr(result);
            return false;
        }
        return true;
    }

    bool VideoDecoder::receiveFrame(FramePtr &frame, std::string &error)
    {
        if (!codec_)
        {
            error = "video decoder is not opened";
            return false;
        }

        frame.reset(av_frame_alloc());
        if (!frame)
        {
            error = "av_frame_alloc failed";
            return false;
        }

        const int result = avcodec_receive_frame(codec_.get(), frame.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            frame.reset();
            return false;
        }
        if (result < 0)
        {
            error = fferr(result);
            frame.reset();
            return false;
        }
        return true;
    }

    void VideoDecoder::flush()
    {
        if (codec_)
            avcodec_flush_buffers(codec_.get());
    }

    void VideoDecoder::drain()
    {
        if (codec_)
            avcodec_send_packet(codec_.get(), nullptr);
    }

    void VideoDecoder::close()
    {
        codec_.reset();
    }
}
