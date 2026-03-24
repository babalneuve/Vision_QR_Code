#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QTranslator>
#include <QtQml>
#include "QrCodeReader.h"
#include "CanHandler.h"

int main(int argc, char *argv[])
{
    // Force v4l2src to use libv4l2, which transparently decodes MJPG to raw
    // YUV in userspace.  Without this, CameraBin's videocrop element cannot
    // link to an MJPG-only webcam source and pipeline negotiation fails.
    if (qgetenv("GST_V4L2_USE_LIBV4L2").isEmpty())
        qputenv("GST_V4L2_USE_LIBV4L2", "1");

    // Enable GStreamer debug logging (level 2 = warnings) so pipeline
    // negotiation issues are visible.  Respect user override if already set.
    if (qgetenv("GST_DEBUG").isEmpty())
        qputenv("GST_DEBUG", "2");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    // Load translation for current locale (e.g. fr.qm for French)
    QTranslator translator;
    if (translator.load(QLocale(), "vision_qr_code", "_", ":/translations"))
        app.installTranslator(&translator);

    qDebug() << "Qt Quick backend:" << qgetenv("QT_QUICK_BACKEND");
    qDebug() << "Available cameras will be listed by QML";

    qmlRegisterType<QrCodeReader>("com.qrcode", 1, 0, "QrCodeReader");

    QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/main_pc.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    // Set up CAN handler (use CAN_INTERFACE env var or default to vcan0)
    QByteArray canEnv = qgetenv("CAN_INTERFACE");
    QString canInterface = canEnv.isEmpty() ? QStringLiteral("vcan0")
                                            : QString::fromLatin1(canEnv);
    CanHandler canHandler(canInterface);

    // Connect QR code reader to CAN handler after QML is loaded
    if (!engine.rootObjects().isEmpty()) {
        QObject *root = engine.rootObjects().first();
        QrCodeReader *qrReader = root->findChild<QrCodeReader *>();
        if (qrReader) {
            QObject::connect(qrReader, &QrCodeReader::qrCodeDetected,
                             &canHandler, &CanHandler::onQrCodeDetected);
            qDebug() << "Connected QrCodeReader to CanHandler";
        } else {
            qWarning() << "QrCodeReader not found in QML tree";
        }
    }

    return app.exec();
}
