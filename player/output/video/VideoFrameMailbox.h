#pragma once
#include "output/video/IVideoSink.h"
#include <QObject>
#include <cstdint>
#include <mutex>
namespace player
{
    class VideoFrameMailbox final : public QObject, public IVideoSink
    {
        Q_OBJECT
    public:
        explicit VideoFrameMailbox(QObject *parent = nullptr);
        void submit(VideoFrame) override;
        void flush() override;
        VideoFrame takeLatest();

    signals:
        void frameAvailable();

    private:
        std::mutex m_;
        VideoFrame latest_;
    };
}
