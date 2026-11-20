#include "core/PlaybackSession.h"

#include <QDebug>
#include <algorithm>

extern "C"
{
    #include <libswscale/swscale.h>
}

namespace player
{
    PlaybackSession::PlaybackSession(
        std::uint64_t id,
        std::uint64_t generation,
        std::shared_ptr<IAudioSink> a,
        std::shared_ptr<VideoFrameMailbox> v,
        PlayerEventCallback e)
        : id_(id)
        , generation_(generation)
        , audio_(std::move(a))
        , video_(std::move(v))
        , event_(std::move(e))
    {
        clock_.setSink(audio_.get());
    }

    PlaybackSession::~PlaybackSession()
    {
        stop();
    }

    void PlaybackSession::setState(PlayerState s)
    {
        {
            std::scoped_lock l(m_);
            snapshot_.state = s;
        }

        if (event_)
            event_({id_, generation(), StateChanged{s}});

    }

    bool PlaybackSession::openAudioDecoder(std::string &error)
    {
        if (!audio_ || demuxer_.info().audioStreamIndex < 0)
            return true;

        const auto *stream = demuxer_.context()->streams[demuxer_.info().audioStreamIndex];
        if (!stream || !stream->codecpar)
        {
            error = "audio stream has no codec parameters";
            return false;
        }

        if (!audioDecoder_.open(stream->codecpar, error))
            return false;

        targetAudioFormat_ = audioDecoder_.format();
        if (!audio_->open(targetAudioFormat_))
        {
            error = "SDL audio output could not be opened";
            audioDecoder_.close();
            return false;
        }

        return true;
    }

    bool PlaybackSession::openVideoDecoder(std::string &error)
    {
        if (demuxer_.info().videoStreamIndex < 0)
            return true;

        const auto *context = demuxer_.context();
        if (!context)
        {
            error = "video demuxer context is null";
            return false;
        }
        const auto *stream = context->streams[demuxer_.info().videoStreamIndex];
        if (!stream || !stream->codecpar)
        {
            error = "video stream has no codec parameters";
            return false;
        }
        return videoDecoder_.open(stream->codecpar, error);
    }

    void PlaybackSession::startDemuxThread()
    {
        demuxStop_ = false;
        demuxEof_ = false;
        demuxFailed_ = false;
        demuxThread_ = std::thread([this]
                                   {
                                       runDemuxLoop();
                                   });
    }

    void PlaybackSession::stopDemuxThread()
    {
        demuxStop_ = true;
        demuxer_.interrupt();
        demuxer_.abortQueues();

        if (demuxThread_.joinable())
            demuxThread_.join();
    }

    void PlaybackSession::startAudioThread()
    {
        audioStop_ = false;
        audioThread_ = std::thread([this]
                                   {
                                       runAudioLoop();
                                   });
    }

    void PlaybackSession::stopAudioThread()
    {
        audioStop_ = true;
        demuxer_.abortQueues();

        if (audioThread_.joinable())
            audioThread_.join();
    }

    void PlaybackSession::startVideoThread()
    {
        videoStop_ = false;
        videoThread_ = std::thread([this]
                                   {
                                       runVideoLoop();
                                   });
    }

    void PlaybackSession::stopVideoThread()
    {
        videoStop_ = true;
        audioControlCv_.notify_all();
        demuxer_.abortQueues();
        if (videoThread_.joinable())
            videoThread_.join();
    }

    void PlaybackSession::runDemuxLoop()
    {
        std::string err;
        std::uint64_t packetCount = 0;
        auto lastDemuxLog = std::chrono::steady_clock::now();
        while (!demuxStop_)
        {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastDemuxLog >= std::chrono::seconds(1))
            {
                qInfo() << "Demux progress / reading next packet:"
                        << "packets=" << packetCount
                        << "audioQueue=" << demuxer_.audioQueue().size()
                        << "videoQueue=" << demuxer_.videoQueue().size()
                        << "session=" << id_ << "generation=" << generation();
                lastDemuxLog = now;
            }

            if (!demuxer_.readNextPacket(err))
            {
                if (!demuxStop_ && err.empty())
                    demuxEof_ = true;
                else if (!demuxStop_)
                {
                    demuxFailed_ = true;
                    qDebug() << "Demux error:" << QString::fromStdString(err);
                    setState(PlayerState::Error);
                    if (event_)
                        event_({id_, generation(), ErrorEvent{-4, err}});
                }

                demuxer_.closeQueues();
                break;
            }
            ++packetCount;
        }
    }

    bool PlaybackSession::waitForAudioCapacity()
    {
        constexpr auto maxQueuedAudio = std::chrono::milliseconds(500);
        while (!audioStop_)
        {
            std::unique_lock lock(audioControlMutex_);
            audioControlCv_.wait(lock, [this]
                                 { return audioStop_ || !audioPaused_; });
            if (audioStop_)
                return false;
            lock.unlock();

            if (!audio_ || audio_->queuedTime() <= maxQueuedAudio)
                return true;

            lock.lock();
            audioControlCv_.wait_for(lock, std::chrono::milliseconds(10), [this]
                                     { return audioStop_ || audioPaused_; });
        }
        return false;
    }

    bool PlaybackSession::waitForAudioDrain()
    {
        while (!audioStop_)
        {
            std::unique_lock lock(audioControlMutex_);
            audioControlCv_.wait(lock, [this]
                                 { return audioStop_ || !audioPaused_; });
            if (audioStop_)
                return false;
            lock.unlock();

            if (!audio_ || audio_->queuedTime() == MediaTime::zero())
                return true;

            lock.lock();
            audioControlCv_.wait_for(lock, std::chrono::milliseconds(10), [this]
                                     { return audioStop_ || audioPaused_; });

        }
        return false;
    }

    bool PlaybackSession::waitUntilVideoDue(MediaTime pts)
    {
        constexpr auto earlyTolerance = std::chrono::milliseconds(5);
        auto lastWaitLog = std::chrono::steady_clock::now();
        while (!videoStop_)
        {
            std::unique_lock lock(audioControlMutex_);
            audioControlCv_.wait(lock, [this]
                                 { return videoStop_ || !audioPaused_; });
            if (videoStop_)
                return false;
            lock.unlock();

            const auto clockNow = clock_.now();
            const auto delta = pts - clockNow;
            if (delta <= earlyTolerance)
                return true;

            const auto now = std::chrono::steady_clock::now();
            if (now - lastWaitLog >= std::chrono::seconds(1))
            {
                qDebug() << "Video frame waiting for clock:"
                         << "targetPtsUs=" << pts.count()
                         << "clockUs=" << clockNow.count()
                         << "deltaUs=" << delta.count()
                         << "audioSubmittedBytes=" << audioSubmittedBytes_.load(std::memory_order_relaxed)
                         << "audioPlayedUs=" << (audio_ ? audio_->playedTime().count() : 0)
                         << "audioQueuedUs=" << (audio_ ? audio_->queuedTime().count() : 0)
                         << "session=" << id_ << "generation=" << generation();
                lastWaitLog = now;
            }

            lock.lock();
            audioControlCv_.wait_for(lock, std::min(delta - earlyTolerance,
                                                    std::chrono::duration_cast<MediaTime>(std::chrono::milliseconds(20))),
                                     [this] { return videoStop_ || audioPaused_; });
        }
        return false;
    }

    void PlaybackSession::tryFinishPlayback()
    {
        if (!demuxEof_ || !audioEofDone_ || !videoEofDone_ ||
            snapshot().state != PlayerState::Playing)
            return;

        bool expected = false;
        if (!endEventSent_.compare_exchange_strong(expected, true))
            return;

        audioPaused_ = true;
        audioControlCv_.notify_all();
        if (audio_)
            audio_->pause();
        clock_.pause();
        setState(PlayerState::Ended);
        if (event_)
            event_({id_, generation(), EndOfMedia{}});
    }

    void PlaybackSession::runVideoLoop()
    {
        const auto *format = demuxer_.context();
        if (!format || !videoDecoder_.isOpen())
        {
            videoEofDone_ = true;
            tryFinishPlayback();
            return;
        }

        const auto *stream = format->streams[demuxer_.info().videoStreamIndex];
        const AVRational timeBase = stream->time_base;
        const AVRational frameRate = av_guess_frame_rate(const_cast<AVFormatContext *>(format),
                                                         const_cast<AVStream *>(stream), nullptr);
        const MediaTime frameDuration = frameRate.num > 0 && frameRate.den > 0
                                            ? MediaTime(av_rescale_q(1, av_inv_q(frameRate), AVRational{1, 1000000}))
                                            : std::chrono::milliseconds(33);
        MediaTime lastPts{};
        bool haveLastPts = false;
        MediaTime syncBasePts{};
        MediaTime syncBaseClock{};
        bool haveSyncBase = false;
        std::string error;
        std::uint64_t videoPacketCount = 0;
        std::uint64_t decodedFrameCount = 0;
        std::uint64_t submittedFrameCount = 0;
        auto lastVideoProgressLog = std::chrono::steady_clock::now();
        auto lastVideoQueueLog = lastVideoProgressLog;

        auto outputFrame = [this, stream, timeBase, frameDuration, &lastPts, &haveLastPts, &syncBasePts, &syncBaseClock, &haveSyncBase, &error, &videoPacketCount, &decodedFrameCount, &submittedFrameCount, &lastVideoProgressLog](const AVFrame &decoded)
        {
            const int width = decoded.width;
            const int height = decoded.height;
            if (width <= 0 || height <= 0)
                return true;

            videoScaler_ = sws_getCachedContext(videoScaler_, width, height,
                                                static_cast<AVPixelFormat>(decoded.format),
                                                width, height, AV_PIX_FMT_RGBA,
                                                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!videoScaler_)
            {
                error = "sws_getCachedContext failed for video frame";
                return false;
            }

            auto pixels = std::make_shared<std::vector<std::uint8_t>>(
                static_cast<std::size_t>(width) * height * 4);
            std::uint8_t *destination[] = {pixels->data(), nullptr, nullptr, nullptr};
            int destinationStride[] = {width * 4, 0, 0, 0};
            const int scaledRows = sws_scale(videoScaler_, decoded.data, decoded.linesize,
                                             0, height, destination, destinationStride);
            if (scaledRows != height)
            {
                error = "sws_scale returned an incomplete video frame";
                return false;
            }

            auto frame = std::make_shared<VideoFrameData>();
            frame->format = PixelFormat::Rgba;
            frame->width = width;
            frame->height = height;
            frame->sessionId = id_;
            frame->generation = generation();
            frame->color.space = decoded.colorspace;
            frame->color.range = decoded.color_range;
            frame->color.primaries = decoded.color_primaries;
            frame->color.transfer = decoded.color_trc;
            frame->storage = pixels;
            frame->planes.push_back({pixels->data(), width * 4, height});

            std::int64_t timestamp = decoded.best_effort_timestamp;
            if (timestamp == AV_NOPTS_VALUE)
                timestamp = decoded.pts;
            if (timestamp != AV_NOPTS_VALUE)
            {
                auto pts = MediaTime(av_rescale_q(timestamp, timeBase, AVRational{1, 1000000}));
                if (stream->start_time != AV_NOPTS_VALUE)
                    pts -= MediaTime(av_rescale_q(stream->start_time, timeBase, AVRational{1, 1000000}));
                frame->pts = std::max(pts, MediaTime::zero());
            }
            else
            {
                frame->pts = haveLastPts ? lastPts + frameDuration : clock_.now();
            }
            lastPts = frame->pts;
            haveLastPts = true;

            if (!haveSyncBase)
            {
                syncBasePts = frame->pts;
                syncBaseClock = clock_.now();
                haveSyncBase = true;
            }

            const MediaTime scheduledPosition = syncBaseClock + (frame->pts - syncBasePts);

            const auto now = std::chrono::steady_clock::now();
            if (now - lastVideoProgressLog >= std::chrono::seconds(1))
            {
                qInfo() << "Video pipeline progress:"
                        << "packets=" << videoPacketCount
                        << "decodedFrames=" << decodedFrameCount
                        << "submittedFrames=" << submittedFrameCount
                        << "rawPts=" << decoded.pts
                        << "bestEffortPts=" << decoded.best_effort_timestamp
                        << "timeBase=" << timeBase.num << "/" << timeBase.den
                        << "startTime=" << stream->start_time
                        << "framePtsUs=" << frame->pts.count()
                        << "scheduledUs=" << scheduledPosition.count()
                        << "clockUs=" << clock_.now().count()
                        << "size=" << width << "x" << height
                        << "session=" << id_ << "generation=" << generation();
                lastVideoProgressLog = now;
            }

            if (!waitUntilVideoDue(scheduledPosition))
                return false;

            constexpr auto lateThreshold = std::chrono::milliseconds(120);
            if (clock_.now() - scheduledPosition > lateThreshold)
                return true;

            if (video_ && !stopped_.load() && !videoStop_.load())
            {
                video_->submit(std::move(frame));
                ++submittedFrameCount;
            }
            return true;
        };

        while (!videoStop_)
        {
            Packet packet;
            auto &videoQueue = demuxer_.videoQueue();
            if (!videoQueue.popFor(packet, std::chrono::milliseconds(100)))
            {
                if (videoStop_)
                    break;
                if (!videoQueue.closed())
                {
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastVideoQueueLog >= std::chrono::seconds(1))
                    {
                        qInfo() << "Video worker waiting for packet:"
                                << "queueSize=" << videoQueue.size()
                                << "demuxEof=" << demuxEof_.load()
                                << "demuxFailed=" << demuxFailed_.load()
                                << "packets=" << videoPacketCount
                                << "decodedFrames=" << decodedFrameCount
                                << "session=" << id_ << "generation=" << generation();
                        lastVideoQueueLog = now;
                    }
                    continue;
                }
                if (!demuxFailed_)
                {
                    videoDecoder_.drain();
                    FramePtr delayed;
                    while (!videoStop_ && videoDecoder_.receiveFrame(delayed, error))
                    {
                        if (!outputFrame(*delayed.get()))
                            break;
                    }
                }
                videoEofDone_ = true;
                tryFinishPlayback();
                break;
            }

            ++videoPacketCount;

            error.clear();
            if (!videoDecoder_.sendPacket(packet.get(), error))
            {
                qDebug() << "video sendPacket failed:" << QString::fromStdString(error);
                continue;
            }

            FramePtr decoded;
            error.clear();
            while (!videoStop_ && videoDecoder_.receiveFrame(decoded, error))
            {
                ++decodedFrameCount;
                if (!outputFrame(*decoded.get()))
                {
                    if (!videoStop_)
                    {
                        setState(PlayerState::Error);
                        if (event_)
                            event_({id_, generation(), ErrorEvent{-5, error}});
                        demuxFailed_ = true;
                        demuxStop_ = true;
                        audioStop_ = true;
                        demuxer_.abortQueues();
                        audioControlCv_.notify_all();
                    }
                    videoStop_ = true;
                    break;
                }
            }
            if (!error.empty())
                qWarning() << "video receiveFrame failed:" << QString::fromStdString(error)
                           << "packetCount=" << videoPacketCount
                           << "decodedFrames=" << decodedFrameCount
                           << "session=" << id_ << "generation=" << generation();
        }
    }

    void PlaybackSession::runAudioLoop()
    {
        std::string err;
        lastPositionEvent_ = {};

        auto submitFrame = [this, &err](const AVFrame &decoded)
        {
            if (!audioResampler_.isPrepared())
            {
                if (!audioResampler_.open(decoded, targetAudioFormat_, err))
                {
                    qDebug() << "audio resampler open failed:" << QString::fromStdString(err);
                    return false;
                }
            }

            std::vector<std::byte> pcm;
            if (!audioResampler_.resample(decoded, pcm, err))
            {
                qDebug() << "audio resample failed:" << QString::fromStdString(err);
                return false;
            }

            if (!pcm.empty())
            {
                if (!waitForAudioCapacity())
                    return false;
                if (audio_ && !audio_->write(pcm))
                {
                    qDebug() << "audio sink write failed:"
                             << "pcmBytes=" << pcm.size()
                             << "submittedBytes=" << audioSubmittedBytes_.load(std::memory_order_relaxed)
                             << "playedUs=" << audio_->playedTime().count()
                             << "queuedUs=" << audio_->queuedTime().count();
                    return false;
                }
                if (audio_)
                    audioSubmittedBytes_.fetch_add(pcm.size(), std::memory_order_relaxed);
            }
            return true;
        };

        while (!audioStop_)
        {
            if (!waitForAudioCapacity())
                break;

            Packet pkt;
            if (!demuxer_.audioQueue().pop(pkt, true))
            {
                if (audioStop_ || !demuxer_.audioQueue().closed())
                    continue;

                if (demuxFailed_)
                    break;

                // End of input: flush delayed codec and resampler samples
                // before declaring playback complete.
                audioDecoder_.drain();
                FramePtr tail;
                while (!audioStop_ && audioDecoder_.receiveFrame(tail, err))
                {
                    if (!submitFrame(*tail.get()))
                        break;
                }

                std::vector<std::byte> pcmTail;
                if (!audioStop_ && audioResampler_.drain(pcmTail, err) && !pcmTail.empty() &&
                    waitForAudioCapacity() && audio_)
                {
                    if (audio_->write(pcmTail))
                        audioSubmittedBytes_.fetch_add(pcmTail.size(), std::memory_order_relaxed);
                    else
                        qDebug() << "audio sink tail write failed:"
                                 << "pcmBytes=" << pcmTail.size()
                                 << "submittedBytes=" << audioSubmittedBytes_.load(std::memory_order_relaxed)
                                 << "playedUs=" << audio_->playedTime().count()
                                 << "queuedUs=" << audio_->queuedTime().count();
                }

                if (!audioStop_ && waitForAudioDrain())
                {
                    // qDebug() << "Audio EOF drain reached:" << "clockUs=" << clock_.now().count()
                    //          << "audioPlayedUs=" << (audio_ ? audio_->playedTime().count() : 0)
                    //          << "audioQueuedUs=" << (audio_ ? audio_->queuedTime().count() : 0)
                    //          << "session=" << id_ << "generation=" << generation();
                    audioEofDone_ = true;
                    tryFinishPlayback();
                }
                break;
            }

            if (!audioDecoder_.isOpen())
            {
                qDebug() << "Audio decoder is not open";
                continue;
            }

            if (!audioDecoder_.sendPacket(pkt.get(), err))
            {
                qDebug() << "audio sendPacket failed:" << QString::fromStdString(err);
                continue;
            }

            FramePtr frame;
            while (!audioStop_ && audioDecoder_.receiveFrame(frame, err))
            {
                if (!submitFrame(*frame.get()))
                    break;
            }
        }
    }

    bool PlaybackSession::open(const MediaSource &s)
    {
        setState(PlayerState::Opening);

        std::string err;
        if (!demuxer_.open(s, err))
        {
            qDebug() << "Failed to open media source:" << QString::fromStdString(err);
            setState(PlayerState::Error);
            if (event_)
                event_({id_, generation(), ErrorEvent{-1, err}});
            return false;
        }

        {
            std::scoped_lock l(m_);
            snapshot_.duration = demuxer_.info().duration;
            snapshot_.seekable = demuxer_.info().seekable;
            snapshot_.hasAudio = demuxer_.info().hasAudio;
            snapshot_.hasVideo = demuxer_.info().hasVideo;
        }

        if (!openAudioDecoder(err))
        {
            qDebug() << "Failed to initialize audio output:" << QString::fromStdString(err);
            setState(PlayerState::Error);
            if (event_)
                event_({id_, generation(), ErrorEvent{-3, err}});
            return false;
        }

        if (!openVideoDecoder(err))
        {
            qDebug() << "Failed to initialize video decoder:" << QString::fromStdString(err);
            setState(PlayerState::Error);
            if (event_)
                event_({id_, generation(), ErrorEvent{-6, err}});
            return false;
        }

        clock_.setSink(demuxer_.info().hasAudio && audioDecoder_.isOpen() ? audio_.get() : nullptr);
        audioEofDone_ = !audioDecoder_.isOpen();
        videoEofDone_ = !videoDecoder_.isOpen();

        if (demuxer_.info().duration.count() > 0)
        {
            if (event_)
            {
                event_({id_, generation(), DurationChanged{demuxer_.info().duration}});
            }
        }

        setState(PlayerState::Ready);

        return true;
    }

    void PlaybackSession::play()
    {
        if (stopped_.load())
            return;

        const auto currentState = snapshot().state;
        if (currentState != PlayerState::Ready && currentState != PlayerState::Seeking)
            return;

        // A repeated Play while already active must not close/recreate the SDL
        // stream while the audio worker may be writing to it.
        if (snapshot().state == PlayerState::Playing)
            return;

        audioPaused_ = false;
        audioControlCv_.notify_all();
        audioEofDone_ = !audioDecoder_.isOpen();
        videoEofDone_ = !videoDecoder_.isOpen();
        endEventSent_ = false;

        if (!demuxThread_.joinable())
            startDemuxThread();

        if (audioDecoder_.isOpen() && !audioThread_.joinable())
            startAudioThread();
        else if (audioThread_.joinable())
            audioStop_ = false;
        if (videoDecoder_.isOpen() && !videoThread_.joinable())
            startVideoThread();

        clock_.resume();
        setState(PlayerState::Playing);

        if (audio_ && audioDecoder_.isOpen())
            audio_->start();
        tryFinishPlayback();
    }

    void PlaybackSession::resume()
    {
        if (stopped_.load())
            return;

        if (snapshot().state != PlayerState::Paused)
            return;

        audioPaused_ = false;
        audioControlCv_.notify_all();

        // A seek stops and joins both workers even when playback was paused.
        // Recreate them before restarting the clock and audio device.
        if (!demuxThread_.joinable())
            startDemuxThread();
        if (audioDecoder_.isOpen() && !audioThread_.joinable())
            startAudioThread();
        if (videoDecoder_.isOpen() && !videoThread_.joinable())
            startVideoThread();

        clock_.resume();
        setState(PlayerState::Playing);

        if (audio_ && audioDecoder_.isOpen())
            audio_->start();
        tryFinishPlayback();
    }

    void PlaybackSession::pause()
    {
        if (snapshot().state != PlayerState::Playing)
            return;

        audioPaused_ = true;
        audioControlCv_.notify_all();

        if (audio_)
            audio_->pause();

        clock_.pause();

        setState(PlayerState::Paused);
    }

    void PlaybackSession::stop()
    {
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true))
            return;

        audioPaused_ = true;
        demuxStop_ = true;
        audioStop_ = true;
        videoStop_ = true;
        audioControlCv_.notify_all();
        generation_++;
        setState(PlayerState::Stopping);

        demuxer_.interrupt();
        demuxer_.abortQueues();
        demuxer_.closeQueues();

        if (demuxThread_.joinable())
            demuxThread_.join();
        if (audioThread_.joinable())
            audioThread_.join();
        if (videoThread_.joinable())
            videoThread_.join();

        if (audio_)
            audio_->stop();
        if (video_)
            video_->flush();

        demuxer_.close();
        audioDecoder_.close();
        videoDecoder_.close();
        audioResampler_.reset();
        if (videoScaler_)
        {
            sws_freeContext(videoScaler_);
            videoScaler_ = nullptr;
        }
        clock_.reset({});
        lastPositionEvent_ = {};

        setState(PlayerState::Idle);
    }

    void PlaybackSession::seek(MediaTime p)
    {
        if (stopped_.load())
            return;

        const auto previousState = snapshot().state;
        if (!demuxer_.info().seekable ||
            (previousState != PlayerState::Ready &&
             previousState != PlayerState::Playing &&
             previousState != PlayerState::Paused))
            return;

        const auto duration = demuxer_.info().duration;
        p = std::max(p, MediaTime::zero());
        if (duration > MediaTime::zero())
            p = std::min(p, duration);

        generation_++;
        audioPaused_ = true;
        audioControlCv_.notify_all();
        setState(PlayerState::Seeking);

        stopDemuxThread();
        stopAudioThread();
        stopVideoThread();

        if (audio_)
            audio_->flush();
        if (video_)
            video_->flush();

        std::string err;
        if (!demuxer_.seek(p, err))
        {
            setState(PlayerState::Error);
            if (event_)
                event_({id_, generation(), ErrorEvent{-2, err}});
            return;
        }

        demuxer_.resetQueues();
        demuxEof_ = false;
        demuxFailed_ = false;
        audioSubmittedBytes_ = 0;
        audioEofDone_ = !audioDecoder_.isOpen();
        videoEofDone_ = !videoDecoder_.isOpen();
        endEventSent_ = false;
        audioDecoder_.flush();
        videoDecoder_.flush();
        audioResampler_.reset();
        clock_.reset(p);
        lastPositionEvent_ = {};

        if (event_)
            event_({id_, generation(), PositionChanged{clock_.now()}});

        if (previousState == PlayerState::Playing)
            play();
        else
            setState(previousState);
    }

    PlayerSnapshot PlaybackSession::snapshot() const
    {
        std::scoped_lock l(m_);

        auto s = snapshot_;
        s.position = clock_.now();
        s.duration = demuxer_.info().duration;
        s.seekable = demuxer_.info().seekable;
        s.hasAudio = demuxer_.info().hasAudio;
        s.hasVideo = demuxer_.info().hasVideo;

        return s;
    }
}
