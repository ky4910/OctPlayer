#include "platform/qt/QtPlayerAdapter.h"

#include <QDebug>
#include <QMetaObject>

namespace player
{
    QtPlayerAdapter::QtPlayerAdapter(QObject *p) : QObject(p)
    {
        player_.setEventCallback([this](auto &e)
                                 { onEvent(e); });

        positionTimer_.setTimerType(Qt::PreciseTimer);
        positionTimer_.setInterval(50);
        connect(&positionTimer_, &QTimer::timeout, this, &QtPlayerAdapter::updatePosition);
        positionTimer_.stop();
    }

    void QtPlayerAdapter::open(const QString &u)
    {
        lastPositionMs_ = -1;
        positionTimer_.stop();
        player_.open({u.toUtf8().toStdString(), {}});
        emit positionChanged();
        emit durationChanged();
        emit seekableChanged();
        emit stateChanged();
    }

    void QtPlayerAdapter::play(){ player_.play(); }
    void QtPlayerAdapter::resume() { player_.resume(); }
    void QtPlayerAdapter::pause() { player_.pause(); }
    void QtPlayerAdapter::stop() { player_.stop(); }
    void QtPlayerAdapter::seek(qint64 ms) { player_.seek(std::chrono::milliseconds(ms)); }
    int QtPlayerAdapter::state() const { return static_cast<int>(player_.snapshot().state); }

    qint64 QtPlayerAdapter::positionMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            player_.snapshot().position).count();
    }

    qint64 QtPlayerAdapter::durationMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            player_.snapshot().duration).count();
    }

    bool QtPlayerAdapter::seekable() const
    {
        return player_.snapshot().seekable;
    }

    void QtPlayerAdapter::updatePosition()
    {
        const auto nowMs = positionMs();
        if (nowMs == lastPositionMs_)
            return;

        lastPositionMs_ = nowMs;

        emit positionChanged();
    }

    void QtPlayerAdapter::onEvent(const PlayerEvent &e)
    {
        QMetaObject::invokeMethod(
            this,
            [this, e]
            {
                if (std::holds_alternative<StateChanged>(e.payload))
                {
                    const auto s = static_cast<int>(state());
                    if (s == static_cast<int>(PlayerState::Playing))
                    {
                        lastPositionMs_ = -1;
                        positionTimer_.start();
                    }
                    else
                    {
                        positionTimer_.stop();
                    }
                    emit stateChanged();
                    emit durationChanged();
                    emit seekableChanged();
                    emit positionChanged();
                }
                else if (std::holds_alternative<DurationChanged>(e.payload))
                {
                    qDebug() << "Duration changed event received for session:" << e.sessionId << ", duration:" << durationMs() << "ms";
                    emit durationChanged();
                    emit seekableChanged();
                }
                else if (auto *position = std::get_if<PositionChanged>(&e.payload))
                {
                    lastPositionMs_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                        position->position).count();
                    emit positionChanged();
                }
                else if (auto *er = std::get_if<ErrorEvent>(&e.payload))
                    emit errorOccurred(QString::fromStdString(er->message));
            },
            Qt::QueuedConnection);
    }

}
