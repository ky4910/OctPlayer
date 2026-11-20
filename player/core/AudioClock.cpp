#include "core/AudioClock.h"

#include <QDebug>
#include <algorithm>

namespace player
{
    MediaTime AudioClock::now() const
    {
        std::scoped_lock l(m_);

        if (paused_)
            return baseMedia_;

        if (sink_)
        {
            const auto played = sink_->playedTime();
            const auto delta = std::max(played - sinkAnchor_, MediaTime::zero());
            return baseMedia_ + MediaTime(static_cast<MediaTime::rep>(delta.count() * rate_));
        }

        const auto elapsed = std::chrono::duration_cast<MediaTime>(
            std::chrono::steady_clock::now() - resumeAnchor_);

        return baseMedia_ + MediaTime(static_cast<MediaTime::rep>(elapsed.count() * rate_));
    }

    void AudioClock::reset(MediaTime t)
    {
        std::scoped_lock l(m_);
        baseMedia_ = t;
        paused_ = true;
        resumeAnchor_ = std::chrono::steady_clock::now();
        sinkAnchor_ = sink_ ? sink_->playedTime() : MediaTime{};
    }

    void AudioClock::pause()
    {
        std::scoped_lock l(m_);
        if (paused_)
        {
            return;
        }

        if (sink_)
        {
            const auto played = sink_->playedTime();
            const auto delta = std::max(played - sinkAnchor_, MediaTime::zero());
            baseMedia_ += MediaTime(static_cast<MediaTime::rep>(delta.count() * rate_));
        }
        else
        {
            const auto elapsed = std::chrono::duration_cast<MediaTime>(
                std::chrono::steady_clock::now() - resumeAnchor_);
            baseMedia_ += MediaTime(static_cast<MediaTime::rep>(elapsed.count() * rate_));
        }
        paused_ = true;
    }

    void AudioClock::resume()
    {
        std::scoped_lock l(m_);

        if (!paused_)
            return;

        resumeAnchor_ = std::chrono::steady_clock::now();
        sinkAnchor_ = sink_ ? sink_->playedTime() : MediaTime{};
        paused_ = false;
    }

    void AudioClock::setRate(double rate)
    {
        std::scoped_lock l(m_);
        if (!paused_)
        {
            if (sink_)
            {
                const auto played = sink_->playedTime();
                const auto delta = std::max(played - sinkAnchor_, MediaTime::zero());
                baseMedia_ += MediaTime(static_cast<MediaTime::rep>(delta.count() * rate_));
            }
            else
            {
                const auto elapsed = std::chrono::duration_cast<MediaTime>(
                    std::chrono::steady_clock::now() - resumeAnchor_);
                baseMedia_ += MediaTime(static_cast<MediaTime::rep>(elapsed.count() * rate_));
            }
        }

        rate_ = rate;
        resumeAnchor_ = std::chrono::steady_clock::now();
        sinkAnchor_ = sink_ ? sink_->playedTime() : MediaTime{};
    }
}

