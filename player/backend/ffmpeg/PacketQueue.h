#pragma once

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "backend/ffmpeg/FfmpegRaii.h"

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <limits>

namespace player
{
    class Packet
    {
    public:
        Packet() = default;
        explicit Packet(PacketPtr pkt);
        Packet(Packet &&) noexcept = default;
        Packet &operator=(Packet &&) noexcept = default;
        Packet(const Packet &) = delete;
        Packet &operator=(const Packet &) = delete;

        AVPacket *get() const { return packet_.get(); }
        bool empty() const { return !packet_; }
        void reset() { packet_.reset(); }

    private:
        PacketPtr packet_;
    };

    class PacketQueue
    {
    public:
        explicit PacketQueue(std::size_t capacity = std::numeric_limits<std::size_t>::max())
            : capacity_(capacity)
        {
        }

        void push(Packet packet);
        bool pop(Packet &packet, bool block = true);
        bool popFor(Packet &packet, std::chrono::milliseconds timeout);
        void flush();
        void abort();
        void close();
        void reset();

        bool empty() const;
        std::size_t size() const;
        bool aborted() const;
        bool closed() const;

    private:
        mutable std::mutex mutex_;
        std::condition_variable cond_;
        std::deque<Packet> packets_;
        const std::size_t capacity_;
        bool abortRequested_{false};
        bool closed_{false};
    };

    using AudioPacketQueue = PacketQueue;
    using VideoPacketQueue = PacketQueue;
    using SubtitlePacketQueue = PacketQueue;
}
