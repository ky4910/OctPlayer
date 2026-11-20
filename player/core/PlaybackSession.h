#pragma once

#include "api/MediaTypes.h"
#include "api/PlayerEvent.h"
#include "backend/ffmpeg/AudioDecoder.h"
#include "backend/ffmpeg/FfmpegDemuxer.h"
#include "backend/ffmpeg/VideoDecoder.h"
#include "core/AudioClock.h"
#include "output/audio/AudioResampler.h"
#include "output/audio/IAudioSink.h"
#include "output/video/VideoFrameMailbox.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

struct SwsContext;

namespace player
{
    class PlaybackSession
    {
    public:
        PlaybackSession(std::uint64_t id,
                        std::uint64_t generation,
                        std::shared_ptr<IAudioSink>,
                        std::shared_ptr<VideoFrameMailbox>,
                        PlayerEventCallback);
        ~PlaybackSession();
        bool open(const MediaSource &);
        void play();
        void resume();
        void pause();
        void stop();
        void seek(MediaTime);
        PlayerSnapshot snapshot() const;
        std::uint64_t id() const { return id_; }
        std::uint64_t generation() const { return generation_.load(); }

    private:
        void setState(PlayerState);
        bool openAudioDecoder(std::string &error);
        bool openVideoDecoder(std::string &error);
        void startDemuxThread();
        void stopDemuxThread();
        void startAudioThread();
        void stopAudioThread();
        void startVideoThread();
        void stopVideoThread();
        void runDemuxLoop();
        void runAudioLoop();
        void runVideoLoop();
        bool waitForAudioCapacity();
        bool waitForAudioDrain();
        bool waitUntilVideoDue(MediaTime pts);
        void tryFinishPlayback();

        const std::uint64_t id_;
        std::atomic<std::uint64_t> generation_{0};
        mutable std::mutex m_;
        PlayerSnapshot snapshot_;
        FfmpegDemuxer demuxer_;
        AudioDecoder audioDecoder_;
        VideoDecoder videoDecoder_;
        AudioResampler audioResampler_;
        SwsContext *videoScaler_{};
        AudioClock clock_;
        std::shared_ptr<IAudioSink> audio_;
        std::shared_ptr<VideoFrameMailbox> video_;
        PlayerEventCallback event_;
        std::thread demuxThread_;
        std::thread audioThread_;
        std::thread videoThread_;
        std::atomic<bool> demuxStop_{false};
        std::atomic<bool> audioStop_{false};
        std::atomic<bool> videoStop_{false};
        std::atomic<bool> audioPaused_{true};
        std::atomic<bool> stopped_{false};
        std::atomic<bool> demuxEof_{false};
        std::atomic<bool> demuxFailed_{false};
        std::atomic<bool> audioEofDone_{false};
        std::atomic<bool> videoEofDone_{false};
        std::atomic<bool> endEventSent_{false};
        std::atomic<std::uint64_t> audioSubmittedBytes_{0};
        std::mutex audioControlMutex_;
        std::condition_variable audioControlCv_;
        AudioFormat targetAudioFormat_{48000, 2, 2};
        MediaTime lastPositionEvent_{};
    };
}
