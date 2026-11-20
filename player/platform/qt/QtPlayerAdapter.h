#pragma once

#include "core/Player.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace player
{
    class QtPlayerAdapter final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(int state READ state NOTIFY stateChanged)
        Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
        Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
        Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)

    public:
        explicit QtPlayerAdapter(QObject *parent = nullptr);

        Q_INVOKABLE void open(const QString &);
        Q_INVOKABLE void play();
        Q_INVOKABLE void resume();
        Q_INVOKABLE void pause();
        Q_INVOKABLE void stop();
        Q_INVOKABLE void seek(qint64);
        int state() const;
        qint64 positionMs() const;
        qint64 durationMs() const;
        bool seekable() const;
        Q_INVOKABLE QObject *videoMailbox() const { return player_.videoMailbox().get(); }

    signals:
        void stateChanged();
        void positionChanged();
        void durationChanged();
        void seekableChanged();
        void errorOccurred(QString message);

    private:
        void onEvent(const PlayerEvent &);
        void updatePosition();

        Player player_;
        QTimer positionTimer_;
        qint64 lastPositionMs_{-1};
    };
}
