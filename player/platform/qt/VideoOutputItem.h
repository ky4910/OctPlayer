#pragma once
#include "output/video/VideoFrameMailbox.h"
#include <QMetaObject>
#include <QPointer>
#include <QQuickFramebufferObject>
#include <QDebug>
#include <QThread>
namespace player
{
    class VideoOutputItem : public QQuickFramebufferObject
    {
    Q_OBJECT public : explicit VideoOutputItem(QQuickItem *p = nullptr);
        ~VideoOutputItem() override;
        Renderer *createRenderer() const override;
        Q_INVOKABLE void setMailbox(QObject *m)
        {
            if (mailbox_)
                disconnect(mailbox_, nullptr, this, nullptr);
            mailbox_ = qobject_cast<VideoFrameMailbox *>(m);
            if (mailbox_)
            {
                connect(mailbox_, &VideoFrameMailbox::frameAvailable,
                        this, &VideoOutputItem::requestUpdate, Qt::QueuedConnection);
                update();
                QMetaObject::invokeMethod(this, "requestUpdate", Qt::QueuedConnection);
            }
        }

        Q_INVOKABLE void requestUpdate()
        {
            update();
        }

        QPointer<VideoFrameMailbox> mailbox() const { return mailbox_; }

    private:
        QPointer<VideoFrameMailbox> mailbox_;
    };
}
