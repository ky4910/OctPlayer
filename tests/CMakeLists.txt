add_executable(PlaybackSessionTests
    PlaybackSessionTests.cpp
)

target_link_libraries(PlaybackSessionTests
    PRIVATE
        player_core
        GTest::gtest_main
)

add_test(NAME PlaybackSessionTests COMMAND PlaybackSessionTests)
