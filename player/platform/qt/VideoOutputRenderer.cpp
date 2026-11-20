#include "platform/qt/VideoOutputRenderer.h"
#include "platform/qt/VideoOutputItem.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShader>
#include <QDebug>

#include <algorithm>

namespace player
{
    VideoOutputRenderer::VideoOutputRenderer() = default;

    void VideoOutputRenderer::synchronize(QQuickFramebufferObject *o)
    {
        auto *i = static_cast<VideoOutputItem *>(o);
        mailbox_ = i->mailbox();
        if (mailbox_)
        {
            if (auto f = mailbox_->takeLatest())
            {
                frame_ = std::move(f);
            }
            else
            {
                frame_.reset();
            }
        }
        else
        {
            frame_.reset();
        }
    }

    void VideoOutputRenderer::render()
    {
        if (!initialized_)
        {
            initializeOpenGLFunctions();

            const bool coreProfile = QOpenGLContext::currentContext() &&
                                     QOpenGLContext::currentContext()->format().profile() == QSurfaceFormat::CoreProfile;

            const char *vertexShader;
            const char *fragmentShader;
            if (coreProfile)
            {
                vertexShader =
                    "#version 150\n"
                    "in vec2 position;"
                    "in vec2 texCoord;"
                    "out vec2 uv;"
                    "void main() { gl_Position = vec4(position, 0.0, 1.0); uv = texCoord; }";
                fragmentShader =
                    "#version 150\n"
                    "uniform sampler2D frameTexture;"
                    "in vec2 uv;"
                    "out vec4 fragColor;"
                    "void main() { fragColor = texture(frameTexture, uv); }";
            }
            else
            {
                vertexShader =
                    "attribute vec2 position;"
                    "attribute vec2 texCoord;"
                    "varying vec2 uv;"
                    "void main() { gl_Position = vec4(position, 0.0, 1.0); uv = texCoord; }";
                fragmentShader =
                    "uniform sampler2D frameTexture;"
                    "varying vec2 uv;"
                    "void main() { gl_FragColor = texture2D(frameTexture, uv); }";
            }

            if (!program_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader) ||
                !program_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader))
            {
                qDebug() << "Video shader source failed:" << program_.log();
            }
            program_.bindAttributeLocation("position", 0);
            program_.bindAttributeLocation("texCoord", 1);
            if (!program_.link())
                qDebug() << "Video shader link failed:" << program_.log();

            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glGenBuffers(1, &vertexBuffer_);
            vertexArray_.create();
            initialized_ = true;
        }

        auto *fbo = framebufferObject();
        if (!fbo || fbo->width() <= 0 || fbo->height() <= 0)
            return;

        glViewport(0, 0, fbo->width(), fbo->height());
        glClearColor(.035f, .055f, .075f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (frame_ && frame_->format == PixelFormat::Rgba && !frame_->planes.empty() &&
            frame_->planes.front().data && frame_->width > 0 && frame_->height > 0 && program_.isLinked())
        {
            glBindTexture(GL_TEXTURE_2D, texture_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            if (textureWidth_ != frame_->width || textureHeight_ != frame_->height)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_->width, frame_->height,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, frame_->planes.front().data);
                textureWidth_ = frame_->width;
                textureHeight_ = frame_->height;
            }
            else
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_->width, frame_->height,
                                GL_RGBA, GL_UNSIGNED_BYTE, frame_->planes.front().data);
            }

            const float viewportAspect = static_cast<float>(fbo->width()) /
                                         std::max(fbo->height(), 1);
            const float frameAspect = static_cast<float>(frame_->width) / frame_->height;
            float halfWidth = 1.f;
            float halfHeight = 1.f;
            if (frameAspect > viewportAspect)
                halfHeight = viewportAspect / frameAspect;
            else
                halfWidth = frameAspect / viewportAspect;

            const GLfloat vertices[] = {
                -halfWidth, -halfHeight, 0.f, 1.f,
                 halfWidth, -halfHeight, 1.f, 1.f,
                -halfWidth,  halfHeight, 0.f, 0.f,
                 halfWidth,  halfHeight, 1.f, 0.f
            };

            if (vertexArray_.isCreated())
                vertexArray_.bind();
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

            program_.bind();
            program_.setUniformValue("frameTexture", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture_);

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                                  reinterpret_cast<const void *>(2 * sizeof(GLfloat)));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            program_.release();
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (vertexArray_.isCreated())
                vertexArray_.release();

            if (glGetError() != GL_NO_ERROR)
                qDebug() << "Video renderer OpenGL error:" << glGetError();
        }
        else if (frame_)
        {
            qDebug() << "Renderer rejected frame: format="
                     << static_cast<int>(frame_->format)
                     << "programLinked=" << program_.isLinked();
        }
    }
}
