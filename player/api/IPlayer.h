#pragma once
#include "api/MediaTypes.h"
#include "api/PlayerEvent.h"

namespace player
{

    class IPlayer
    {
    public:
        virtual ~IPlayer() = default;
        virtual void open(MediaSource source) = 0;
        virtual void play() = 0;
        virtual void resume() = 0;
        virtual void pause() = 0;
        virtual void stop() = 0;
        virtual void seek(MediaTime position) = 0;
        virtual void setVolume(float) = 0;
        virtual void setMuted(bool) = 0;
        virtual void setRate(double) = 0;
        virtual PlayerSnapshot snapshot() const = 0;
        virtual void setEventCallback(PlayerEventCallback) = 0;
    };
}
