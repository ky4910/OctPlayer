find_package(PkgConfig QUIET)

set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/ffmpeg60")

set(CMAKE_BUILD_TYPE Debug)

if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswresample
        libswscale
    )
endif()

add_library(player_api INTERFACE)
target_include_directories(player_api INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(player_core STATIC
    core/Player.cpp
    core/Player.h
    core/PlaybackSession.cpp
    core/PlaybackSession.h
    core/MediaClock.cpp
    core/MediaClock.h
    core/AudioClock.cpp
    core/AudioClock.h
    backend/ffmpeg/FfmpegDemuxer.cpp
    backend/ffmpeg/FfmpegDemuxer.h
    backend/ffmpeg/FfmpegRaii.h
    backend/ffmpeg/PacketQueue.cpp
    backend/ffmpeg/PacketQueue.h
    backend/ffmpeg/AudioDecoder.cpp
    backend/ffmpeg/AudioDecoder.h
    backend/ffmpeg/VideoDecoder.cpp
    backend/ffmpeg/VideoDecoder.h
    output/audio/SdlAudioSink.cpp
    output/audio/SdlAudioSink.h
    output/audio/IAudioSink.h
    output/audio/AudioResampler.cpp
    output/audio/AudioResampler.h
    output/video/VideoFrameMailbox.cpp
    output/video/VideoFrameMailbox.h
    output/video/IVideoSink.h
    platform/qt/QtPlayerAdapter.cpp
    platform/qt/QtPlayerAdapter.h
    platform/qt/VideoOutputItem.cpp
    platform/qt/VideoOutputItem.h
    platform/qt/VideoOutputRenderer.cpp
    platform/qt/VideoOutputRenderer.h
)

target_include_directories(player_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FFMPEG_ROOT}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL3-3.2.18/x86_64-w64-mingw32/include/SDL3
)

target_link_libraries(player_core PUBLIC
    player_api
    SDL3::SDL3
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::OpenGL
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(player_core PUBLIC PkgConfig::FFMPEG)
else()
    if(NOT EXISTS "${FFMPEG_ROOT}/lib/avcodec.lib")
        message(FATAL_ERROR "FFmpeg libraries were not found under ${FFMPEG_ROOT}. Please verify the bundled ThirdParty/ffmpeg60 folder.")
    endif()

    target_link_libraries(player_core PUBLIC
        "${FFMPEG_ROOT}/lib/avformat.lib"
        "${FFMPEG_ROOT}/lib/avcodec.lib"
        "${FFMPEG_ROOT}/lib/avutil.lib"
        "${FFMPEG_ROOT}/lib/swresample.lib"
        "${FFMPEG_ROOT}/lib/swscale.lib"
    )
endif()
