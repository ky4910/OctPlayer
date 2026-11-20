#pragma once
#include "api/MediaTypes.h"
#include "output/video/VideoFrameMailbox.h"
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPointer>
#include <QQuickFramebufferObject>
#include <cstdint>

namespace player
{
    class VideoOutputRenderer final : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
    {
    public:
        VideoOutputRenderer();
        void synchronize(QQuickFramebufferObject *) override;
        void render() override;

    private:
        QPointer<VideoFrameMailbox> mailbox_;
        VideoFrame frame_;
        QOpenGLShaderProgram program_;
        QOpenGLVertexArrayObject vertexArray_;
        GLuint texture_{};
        GLuint vertexBuffer_{};
        int textureWidth_{};
        int textureHeight_{};
        bool initialized_{};
    };
}
