#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include <QQmlContext>
#include <QtQml>
#include "QrCodeReader.h"

/** \brief Main entry point of the application
 *  \param[in] argc  Number of command line parameters
 *  \param[in] argv  List of parameters
 *  \return Error code
 */
int main(int argc, char *argv[])
{
    QObject *cam1 = nullptr;
    QObject *cam2 = nullptr;
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_NativeWindows);

    QGuiApplication app(argc, argv);

    // Register QrCodeReader type for QML
    qmlRegisterType<QrCodeReader>("com.qrcode", 1, 0, "QrCodeReader");

    QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    engine.load(url);

    QObject *root = qobject_cast<QObject*>(engine.rootObjects().first());
    if (!root) {
        qCritical() << "Failed to load root QML object";
        return -1;
    }

    QObject *button1 = root->findChild<QObject*>("ip_button1", Qt::FindChildrenRecursively);
    QObject *button2 = root->findChild<QObject*>("ip_button2", Qt::FindChildrenRecursively);
    QObject *resetButton1 = root->findChild<QObject*>("reset_pipeline_button1", Qt::FindChildrenRecursively);
    QObject *stopButton1 = root->findChild<QObject*>("stop_button1", Qt::FindChildrenRecursively);
    QObject *resetButton2 = root->findChild<QObject*>("reset_pipeline_button2", Qt::FindChildrenRecursively);
    QObject *stopButton2 = root->findChild<QObject*>("stop_button2", Qt::FindChildrenRecursively);

    QObject *camera1 = root->findChild<QObject *>("camera1", Qt::FindChildrenRecursively);
    QObject *camera2 = root->findChild<QObject *>("camera2", Qt::FindChildrenRecursively);

    if (camera1)
        cam1 = camera1->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);
    if (camera2)
        cam2 = camera2->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);

    if (!cam1 || !cam2) {
        qCritical() << "Failed to find camera controllers";
        return -1;
    }

    // Eagerly initialize the GStreamer bus so that resetPipeline/releasePipeline
    // don't crash when called before any IP change
    QMetaObject::invokeMethod(cam1, "GetGstBus");
    QMetaObject::invokeMethod(cam2, "GetGstBus");

    if (button1)
        QObject::connect(button1, SIGNAL(ipChange1(QString)), cam1, SLOT(changeIP(QString)));
    if (button2)
        QObject::connect(button2, SIGNAL(ipChange2(QString)), cam2, SLOT(changeIP(QString)));
    if (resetButton1)
        QObject::connect(resetButton1, SIGNAL(resetPipeline1()), cam1, SLOT(resetPipeline()));
    if (stopButton1)
        QObject::connect(stopButton1, SIGNAL(stopPipeline1()), cam1, SLOT(releasePipeline()));
    if (resetButton2)
        QObject::connect(resetButton2, SIGNAL(resetPipeline2()), cam2, SLOT(resetPipeline()));
    if (stopButton2)
        QObject::connect(stopButton2, SIGNAL(stopPipeline2()), cam2, SLOT(releasePipeline()));

    return app.exec();
}
