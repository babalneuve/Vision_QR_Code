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
    // resetPipeline() and releasePipeline() are Q_INVOKABLE (not slots),
    // so use index-based QMetaObject::connect() instead of SIGNAL/SLOT macros.
    if (resetButton1) {
        int sig = resetButton1->metaObject()->indexOfSignal("resetPipeline1()");
        int method = cam1->metaObject()->indexOfMethod("resetPipeline()");
        if (sig >= 0 && method >= 0)
            QMetaObject::connect(resetButton1, sig, cam1, method);
    }
    if (stopButton1) {
        int sig = stopButton1->metaObject()->indexOfSignal("stopPipeline1()");
        int method = cam1->metaObject()->indexOfMethod("releasePipeline()");
        if (sig >= 0 && method >= 0)
            QMetaObject::connect(stopButton1, sig, cam1, method);
    }
    if (resetButton2) {
        int sig = resetButton2->metaObject()->indexOfSignal("resetPipeline2()");
        int method = cam2->metaObject()->indexOfMethod("resetPipeline()");
        if (sig >= 0 && method >= 0)
            QMetaObject::connect(resetButton2, sig, cam2, method);
    }
    if (stopButton2) {
        int sig = stopButton2->metaObject()->indexOfSignal("stopPipeline2()");
        int method = cam2->metaObject()->indexOfMethod("releasePipeline()");
        if (sig >= 0 && method >= 0)
            QMetaObject::connect(stopButton2, sig, cam2, method);
    }

    return app.exec();
}
