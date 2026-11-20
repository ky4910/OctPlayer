#include "core/MediaClock.h"
namespace player
{
    MediaTime MediaClock::now() const
    {
        std::scoped_lock l(m_);
        if (paused_)
            return baseMedia_;
        auto d = std::chrono::steady_clock::now() - baseWall_;
        return baseMedia_ + MediaTime(static_cast<MediaTime::rep>(std::chrono::duration_cast<MediaTime>(d).count() * rate_));
    }
    void MediaClock::reset(MediaTime t)
    {
        std::scoped_lock l(m_);
        baseMedia_ = t;
        baseWall_ = std::chrono::steady_clock::now();
    }
    void MediaClock::pause()
    {
        std::scoped_lock l(m_);
        if (!paused_)
        {
            auto d = std::chrono::steady_clock::now() - baseWall_;
            baseMedia_ += MediaTime(static_cast<MediaTime::rep>(std::chrono::duration_cast<MediaTime>(d).count() * rate_));
            paused_ = true;
        }
    }
    void MediaClock::resume()
    {
        std::scoped_lock l(m_);
        if (paused_)
        {
            baseWall_ = std::chrono::steady_clock::now();
            paused_ = false;
        }
    }
    void MediaClock::setRate(double r)
    {
        auto t = now();
        std::scoped_lock l(m_);
        baseMedia_ = t;
        baseWall_ = std::chrono::steady_clock::now();
        rate_ = r;
    }
}
