#include "core/PlaybackSession.h"

#include <gtest/gtest.h>

namespace
{
    using namespace player;

    TEST(PlaybackSessionTests, StateTransitionsWithoutOutputs)
    {
        PlaybackSession session(7, nullptr, nullptr, {});

        EXPECT_EQ(session.snapshot().state, PlayerState::Idle);

        session.play();
        EXPECT_EQ(session.snapshot().state, PlayerState::Playing);

        session.pause();
        EXPECT_EQ(session.snapshot().state, PlayerState::Paused);

        session.stop();
        EXPECT_EQ(session.snapshot().state, PlayerState::Idle);
    }

    TEST(PlaybackSessionTests, OpenInvalidUriReportsErrorEvent)
    {
        std::vector<PlayerEvent> events;
        PlaybackSession session(
            8,
            nullptr,
            std::make_shared<VideoFrameMailbox>(),
            [&events](const PlayerEvent &e)
            {
                events.push_back(e);
            });

        MediaSource source{"this-file-does-not-exist.mp4", {}};
        EXPECT_FALSE(session.open(source));
        EXPECT_EQ(session.snapshot().state, PlayerState::Error);

        ASSERT_FALSE(events.empty());
        EXPECT_EQ(events.back().sessionId, 8u);
        EXPECT_TRUE(std::holds_alternative<ErrorEvent>(events.back().payload));
    }
}
