#pragma once
#include "api/MediaTypes.h"

#include <chrono>
#include <mutex>

namespace player
{
    class MediaClock
    {
    public:
        MediaTime now() const;
        void reset(MediaTime);
        void pause();
        void resume();
        void setRate(double);

    private:
        mutable std::mutex m_;
        MediaTime baseMedia_{};
        std::chrono::steady_clock::time_point baseWall_{std::chrono::steady_clock::now()};
        bool paused_{true};
        double rate_{1.0};
    };
}
