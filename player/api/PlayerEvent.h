#pragma once

#include "api/MediaTypes.h"

#include <functional>
#include <string>
#include <variant>

namespace player
{
    struct StateChanged
    {
        PlayerState state;
    };

    struct PositionChanged
    {
        MediaTime position;
    };

    struct DurationChanged
    {
        MediaTime duration;
    };

    struct ErrorEvent
    {
        int code{};
        std::string message;
    };

    struct EndOfMedia
    {
    };

    using PlayerEventPayload = std::variant<
        StateChanged,
        PositionChanged,
        DurationChanged,
        ErrorEvent,
        EndOfMedia>;

    struct PlayerEvent
    {
        std::uint64_t sessionId{};
        std::uint64_t generation{};
        PlayerEventPayload payload;
    };

    using PlayerEventCallback = std::function<void(const PlayerEvent &)>;
}
