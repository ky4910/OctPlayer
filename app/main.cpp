#include "platform/qt/QtPlayerAdapter.h"
#include "platform/qt/VideoOutputItem.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

int main(int argc, char **argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);

    using namespace Qt::StringLiterals;

    qmlRegisterType<player::VideoOutputItem>("OctPlayer", 1, 0, "VideoOutput");

    player::QtPlayerAdapter player;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("player", &player);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
