#pragma once

#include "api/MediaTypes.h"
#include "output/audio/IAudioSink.h"

#include <chrono>
#include <mutex>

namespace player
{
    class AudioClock
    {
    public:
        AudioClock() = default;

        void setSink(const IAudioSink *sink)
        {
            std::scoped_lock l(m_);
            sink_ = sink;
            sinkAnchor_ = sink_ ? sink_->playedTime() : MediaTime{};
        }

        MediaTime now() const;
        void reset(MediaTime t);
        void pause();
        void resume();
        void setRate(double rate);

    private:
        mutable std::mutex m_;
        const IAudioSink *sink_{nullptr};
        MediaTime baseMedia_{};
        MediaTime sinkAnchor_{};
        std::chrono::steady_clock::time_point resumeAnchor_{};
        bool paused_{true};
        double rate_{1.0};
    };
}
