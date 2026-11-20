#include "backend/ffmpeg/AudioDecoder.h"

#include <QDebug>

extern "C"
{
    #include <libavutil/error.h>
}

namespace player
{
    std::string AudioDecoder::fferr(int code)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(code, buffer, sizeof buffer);
        return buffer;
    }

    AudioDecoder::~AudioDecoder()
    {
        close();
    }

    bool AudioDecoder::open(const AVCodecParameters *params, std::string &error)
    {
        close();
        if (!params)
        {
            error = "codec parameters is null";
            return false;
        }

        const AVCodec *codec = avcodec_find_decoder(params->codec_id);
        if (!codec)
        {
            error = "avcodec_find_decoder failed";
            return false;
        }

        auto ctx = CodecPtr(avcodec_alloc_context3(codec));
        if (!ctx)
        {
            error = "avcodec_alloc_context3 failed";
            return false;
        }

        const int ret = avcodec_parameters_to_context(ctx.get(), params);
        if (ret < 0)
        {
            error = fferr(ret);
            return false;
        }

        if (avcodec_open2(ctx.get(), codec, nullptr) < 0)
        {
            error = "avcodec_open2 failed";
            return false;
        }

        codec_ = std::move(ctx);
        format_.sampleRate = codec_->sample_rate > 0 ? codec_->sample_rate : 48000;
        format_.channels = codec_->ch_layout.nb_channels > 0 ? codec_->ch_layout.nb_channels : 2;
        format_.bytesPerSample = 2;

        qDebug("AudioDecoder opened: codec=%s, sampleRate=%d, channels=%d",
            codec->name,
            format_.sampleRate,
            format_.channels);

        return true;
    }

    bool AudioDecoder::sendPacket(const AVPacket *pkt, std::string &error)
    {
        if (!codec_)
        {
            error = "decoder is not opened";
            return false;
        }

        const int ret = avcodec_send_packet(codec_.get(), pkt);
        if (ret < 0)
        {
            error = fferr(ret);
            return false;
        }

        return true;
    }

    bool AudioDecoder::receiveFrame(FramePtr &frame, std::string &error)
    {
        if (!codec_)
        {
            error = "decoder is not opened";
            return false;
        }

        frame.reset(av_frame_alloc());
        if (!frame)
        {
            error = "av_frame_alloc failed";
            return false;
        }

        const int ret = avcodec_receive_frame(codec_.get(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            frame.reset();
            return false;
        }

        if (ret < 0)
        {
            error = fferr(ret);
            frame.reset();
            return false;
        }

        return true;
    }

    void AudioDecoder::flush()
    {
        if (codec_)
            avcodec_flush_buffers(codec_.get());
    }

    void AudioDecoder::drain()
    {
        if (codec_)
            avcodec_send_packet(codec_.get(), nullptr);
    }

    void AudioDecoder::close()
    {
        codec_.reset();
    }
}
