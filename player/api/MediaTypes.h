#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace player
{
    using MediaTime = std::chrono::microseconds;

    struct MediaSource
    {
        std::string uri;
        std::unordered_map<std::string, std::string> options;
    };

    enum class PlayerState
    {
        Idle,
        Opening,
        Ready,
        Playing,
        Paused,
        Seeking,
        Buffering,
        Stopping,
        Ended,
        Error
    };

    enum class PixelFormat
    {
        Unknown,
        Yuv420p,
        Nv12,
        Rgba,
        HardwareSurface
    };

    struct ColorInfo
    {
        int space{};
        int range{};
        int primaries{};
        int transfer{};
    };

    struct VideoPlane
    {
        const std::uint8_t *data{};
        int stride{};
        int height{};
    };

    struct VideoFrameData
    {
        PixelFormat format{PixelFormat::Unknown};
        int width{};
        int height{};
        MediaTime pts{};
        std::uint64_t sessionId{};
        std::uint64_t generation{};
        ColorInfo color{};
        std::vector<VideoPlane> planes;
        std::shared_ptr<void> storage;
    };

    using VideoFrame = std::shared_ptr<const VideoFrameData>;

    struct AudioFormat
    {
        int sampleRate{48000};
        int channels{2};
        int bytesPerSample{2};
    };

    struct MediaInfo
    {
        MediaTime duration{};
        int audioStreamIndex{-1};
        int videoStreamIndex{-1};
        int width{0};
        int height{0};
        int sampleRate{0};
        int channels{0};
        bool seekable{false};
        bool hasAudio{false};
        bool hasVideo{false};
    };

    struct PlayerSnapshot
    {
        PlayerState state{PlayerState::Idle};
        MediaTime position{};
        MediaTime duration{};
        bool seekable{};
        bool hasAudio{};
        bool hasVideo{};
        bool muted{};
        float volume{1.0f};
        double rate{1.0};
    };

}
