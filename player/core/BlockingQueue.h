#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace player
{
    template <class T>
    class BlockingQueue
    {
    public:
        explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}
        bool push(T value)
        {
            std::unique_lock lock(m_);
            space_.wait(lock, [&]
                        { return aborted_ || closed_ || q_.size() < capacity_; });
            if (aborted_ || closed_)
                return false;
            q_.push_back(std::move(value));
            data_.notify_one();
            return true;
        }
        std::optional<T> pop()
        {
            std::unique_lock lock(m_);
            data_.wait(lock, [&]
                       { return aborted_ || closed_ || !q_.empty(); });
            if (aborted_ || q_.empty())
                return std::nullopt;
            T value = std::move(q_.front());
            q_.pop_front();
            space_.notify_one();
            return value;
        }
        void flush()
        {
            std::scoped_lock lock(m_);
            q_.clear();
            space_.notify_all();
        }
        void abort()
        {
            std::scoped_lock lock(m_);
            aborted_ = true;
            data_.notify_all();
            space_.notify_all();
        }
        void reset()
        {
            std::scoped_lock lock(m_);
            aborted_ = false;
            closed_ = false;
        }
        void close()
        {
            std::scoped_lock lock(m_);
            closed_ = true;
            data_.notify_all();
            space_.notify_all();
        }

    private:
        std::mutex m_;
        std::condition_variable data_, space_;
        std::deque<T> q_;
        std::size_t capacity_;
        bool aborted_{};
        bool closed_{};
    };
}
