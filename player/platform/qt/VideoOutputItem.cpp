#include "platform/qt/VideoOutputItem.h"
#include "platform/qt/VideoOutputRenderer.h"
#include <QDebug>
namespace player
{
    VideoOutputItem::VideoOutputItem(QQuickItem *p) : QQuickFramebufferObject(p)
    {
        // qDebug() << "VideoOutputItem created:" << this;
        setMirrorVertically(true);
    }
    VideoOutputItem::~VideoOutputItem()
    {
        // qDebug() << "VideoOutputItem destroyed:" << this;
    }
    QQuickFramebufferObject::Renderer *VideoOutputItem::createRenderer() const { return new VideoOutputRenderer; }
}
