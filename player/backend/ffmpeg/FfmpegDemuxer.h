#pragma once
#include "api/MediaTypes.h"
#include "backend/ffmpeg/FfmpegRaii.h"
#include "backend/ffmpeg/PacketQueue.h"

#include <string>
#include <atomic>

namespace player
{
    class FfmpegDemuxer
    {
    public:
        bool open(const MediaSource &, std::string &error);
        void close();
        bool seek(MediaTime, std::string &error);
        bool readNextPacket(std::string &error);
        void flushQueues();
        void abortQueues();
        void closeQueues();
        void resetQueues();
        void interrupt();
        void clearInterrupt();
        bool isInterrupted() const { return interruptRequested_.load(); }

        const MediaInfo &info() const { return info_; }
        AVFormatContext *context() const { return format_.get(); }
        AudioPacketQueue &audioQueue() { return audioQueue_; }
        VideoPacketQueue &videoQueue() { return videoQueue_; }
        SubtitlePacketQueue &subtitleQueue() { return subtitleQueue_; }

    private:
        void refreshInfo();

        FormatPtr format_;
        MediaInfo info_;
        AudioPacketQueue audioQueue_{64};
        VideoPacketQueue videoQueue_{64};
        SubtitlePacketQueue subtitleQueue_;
        std::atomic<bool> interruptRequested_{false};
    };
}
