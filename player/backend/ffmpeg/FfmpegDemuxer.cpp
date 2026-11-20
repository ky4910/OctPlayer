#include "backend/ffmpeg/FfmpegDemuxer.h"

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavutil/error.h>
}

namespace player
{
    static int interruptCallback(void *opaque)
    {
        const auto *demuxer = static_cast<const FfmpegDemuxer *>(opaque);
        return demuxer && demuxer->isInterrupted() ? 1 : 0;
    }

    static std::string fferr(int e)
    {
        char b[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(e, b, sizeof b);
        return b;
    }

    void FfmpegDemuxer::refreshInfo()
    {
        info_ = {};
        if (!format_)
            return;

        if (format_->duration != AV_NOPTS_VALUE)
        {
            const auto us = av_rescale_q(format_->duration, AV_TIME_BASE_Q, AVRational{1, 1000000});
            info_.duration = MediaTime(static_cast<MediaTime::rep>(us));
        }

        info_.seekable = format_->pb != nullptr && (format_->pb->seekable & AVIO_SEEKABLE_NORMAL) != 0;

        for (unsigned i = 0; i < format_->nb_streams; ++i)
        {
            const auto *st = format_->streams[i];
            if (!st || !st->codecpar)
                continue;

            if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                info_.videoStreamIndex = static_cast<int>(i);
                info_.width = st->codecpar->width;
                info_.height = st->codecpar->height;
                info_.hasVideo = true;
            }
            else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                info_.audioStreamIndex = static_cast<int>(i);
                info_.sampleRate = st->codecpar->sample_rate;
                info_.channels = st->codecpar->ch_layout.nb_channels;
                info_.hasAudio = true;
            }
        }
    }

    bool FfmpegDemuxer::readNextPacket(std::string &error)
    {
        if (!format_)
        {
            error = "demuxer is not opened";
            return false;
        }

        PacketPtr pkt(av_packet_alloc());
        if (!pkt)
        {
            error = "av_packet_alloc failed";
            return false;
        }

        const int r = av_read_frame(format_.get(), pkt.get());
        if (r < 0)
        {
            if (r == AVERROR_EOF)
                return false;

            error = fferr(r);
            return false;
        }

        const int streamIndex = pkt->stream_index;
        if (info_.audioStreamIndex >= 0 && streamIndex == info_.audioStreamIndex)
        {
            audioQueue_.push(Packet(std::move(pkt)));
            return true;
        }

        if (info_.videoStreamIndex >= 0 && streamIndex == info_.videoStreamIndex)
        {
            videoQueue_.push(Packet(std::move(pkt)));
            return true;
        }

        // Subtitle/data streams have no consumer yet. Discard them instead of
        // accumulating packets indefinitely in the currently unconsumed queue.
        return true;
    }

    void FfmpegDemuxer::flushQueues()
    {
        audioQueue_.flush();
        videoQueue_.flush();
        subtitleQueue_.flush();
    }

    void FfmpegDemuxer::abortQueues()
    {
        audioQueue_.abort();
        videoQueue_.abort();
        subtitleQueue_.abort();
    }

    void FfmpegDemuxer::closeQueues()
    {
        audioQueue_.close();
        videoQueue_.close();
        subtitleQueue_.close();
    }

    void FfmpegDemuxer::resetQueues()
    {
        audioQueue_.reset();
        videoQueue_.reset();
        subtitleQueue_.reset();
    }

    bool FfmpegDemuxer::open(const MediaSource &s, std::string &error)
    {
        close();
        clearInterrupt();

        AVFormatContext *raw = nullptr;
        const int r = avformat_open_input(&raw, s.uri.c_str(), nullptr, nullptr);
        if (r < 0)
        {
            error = fferr(r);
            return false;
        }

        format_.reset(raw);
        format_->interrupt_callback.callback = interruptCallback;
        format_->interrupt_callback.opaque = this;

        const int infoResult = avformat_find_stream_info(format_.get(), nullptr);
        if (infoResult < 0)
        {
            error = fferr(infoResult);
            close();
            return false;
        }

        refreshInfo();
        resetQueues();

        return true;
    }

    void FfmpegDemuxer::close()
    {
        interrupt();
        abortQueues();
        format_.reset();
        info_ = {};
    }

    bool FfmpegDemuxer::seek(MediaTime p, std::string &error)
    {
        if (!format_)
            return false;

        clearInterrupt();

        const int64_t ts = av_rescale_q(p.count(), AVRational{1, 1000000}, AV_TIME_BASE_Q);
        const int r = avformat_seek_file(format_.get(), -1, INT64_MIN, ts, INT64_MAX, 0);
        if (r < 0)
        {
            error = fferr(r);
            return false;
        }

        flushQueues();
        return true;
    }

    void FfmpegDemuxer::interrupt()
    {
        interruptRequested_.store(true);
    }

    void FfmpegDemuxer::clearInterrupt()
    {
        interruptRequested_.store(false);
    }
}
