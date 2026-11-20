#include "output/video/VideoFrameMailbox.h"
#include <QDebug>
#include <utility>

namespace player
{
    VideoFrameMailbox::VideoFrameMailbox(QObject *parent) : QObject(parent) {}

    void VideoFrameMailbox::submit(VideoFrame f)
    {
        {
            std::scoped_lock l(m_);
            latest_ = std::move(f);
            // ++revision_;
        }
        emit frameAvailable();
        // static std::uint64_t notificationCount = 0;
        // if (++notificationCount % 20 == 0)
        //     qDebug() << "VideoFrameMailbox frameAvailable emitted #" << notificationCount;
    }
    void VideoFrameMailbox::flush()
    {
        {
            std::scoped_lock l(m_);
            latest_.reset();
            // ++revision_;
        }
        emit frameAvailable();
    }
    VideoFrame VideoFrameMailbox::takeLatest()
    {
        std::scoped_lock l(m_);
        return std::exchange(latest_, {});
    }
}
