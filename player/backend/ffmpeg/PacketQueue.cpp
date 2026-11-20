#include "backend/ffmpeg/PacketQueue.h"

#include <QDebug>

namespace player
{
    Packet::Packet(PacketPtr pkt)
        : packet_(std::move(pkt))
    {
    }

    void PacketQueue::push(Packet packet)
    {
        if (packet.empty())
            return;

        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]
                   { return abortRequested_ || closed_ || packets_.size() < capacity_; });
        if (abortRequested_ || closed_)
            return;
        packets_.push_back(std::move(packet));
        lock.unlock();

        cond_.notify_one();
    }

    bool PacketQueue::pop(Packet &packet, bool block)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (block)
        {
            cond_.wait(lock, [this]
                       { return abortRequested_ || closed_ || !packets_.empty(); });
        }

        if (packets_.empty())
        {
            packet = Packet{};
            return false;
        }

        packet = std::move(packets_.front());
        packets_.pop_front();
        lock.unlock();
        cond_.notify_all();
        return true;
    }

    bool PacketQueue::popFor(Packet &packet, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait_for(lock, timeout, [this]
                       { return abortRequested_ || closed_ || !packets_.empty(); });

        if (packets_.empty())
        {
            packet = Packet{};
            return false;
        }

        packet = std::move(packets_.front());
        packets_.pop_front();
        lock.unlock();
        cond_.notify_all();
        return true;
    }

    void PacketQueue::flush()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        packets_.clear();
    }

    void PacketQueue::abort()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            abortRequested_ = true;
            packets_.clear();
        }
        cond_.notify_all();
    }

    void PacketQueue::close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cond_.notify_all();
    }

    void PacketQueue::reset()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            abortRequested_ = false;
            closed_ = false;
            packets_.clear();
        }
        cond_.notify_all();
    }

    bool PacketQueue::empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return packets_.empty();
    }

    std::size_t PacketQueue::size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return packets_.size();
    }

    bool PacketQueue::aborted() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return abortRequested_;
    }

    bool PacketQueue::closed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }
}
