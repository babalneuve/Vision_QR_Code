#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QTranslator>
#include <QtQml>
#include "QrCodeReader.h"
#include "CanHandler.h"

extern "C" {
#include <libhal.h>
}

/** \brief Main entry point of the application
 *  \param[in] argc  Number of command line parameters
 *  \param[in] argv  List of parameters
 *  \return Error code
 */
int main(int argc, char *argv[])
{
    QObject *cam1 = nullptr;
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_NativeWindows);

    QGuiApplication app(argc, argv);

    // Initialize Vision 3 HAL
    if (hal_init() != HAL_E_OK) {
        qCritical() << "hal_init() failed";
        return -1;
    }

    // Initialize CAN via Vision 3 HAL
    hal_error canErr = hal_can_init();
    if (canErr != HAL_E_OK)
        qCritical() << "hal_can_init() failed:" << canErr;

    canErr = hal_can_init_channel(HAL_CAN_CHANNEL_0, HAL_CAN_BAUD_250K);
    if (canErr != HAL_E_OK)
        qCritical() << "hal_can_init_channel(0, 250K) failed:" << canErr;

    char *ifname = nullptr;
    QString canInterface = QStringLiteral("can0");
    if (hal_can_get_ifname(HAL_CAN_CHANNEL_0, &ifname) == HAL_E_OK && ifname) {
        canInterface = QString::fromLatin1(ifname);
        qDebug() << "HAL CAN interface name:" << canInterface;
    } else {
        qWarning() << "hal_can_get_ifname failed, falling back to can0";
    }

    // Load translation for current locale (e.g. fr.qm for French)
    QTranslator translator;
    if (translator.load(QLocale(), "vision_qr_code", "_", ":/translations"))
        app.installTranslator(&translator);

    QLoggingCategory::setFilterRules("*.debug=true");
    qDebug() << "Debug logging enabled";

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

    // Only camera 1 is used — control buttons have been removed from the UI
    QObject *camera1 = root->findChild<QObject *>("camera1", Qt::FindChildrenRecursively);

    if (camera1)
        cam1 = camera1->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);

    if (!cam1) {
        qCritical() << "Failed to find camera 1 controller";
        return -1;
    }

    // Initialize GStreamer bus for camera 1 only
    QMetaObject::invokeMethod(cam1, "GetGstBus");

    // Set up CAN handler and connect to QR code reader
    CanHandler canHandler(canInterface);
    QrCodeReader *qrReader = root->findChild<QrCodeReader *>();
    if (qrReader) {
        QObject::connect(qrReader, &QrCodeReader::qrCodeDetected,
                         &canHandler, &CanHandler::onQrCodeDetected);
        qDebug() << "Connected QrCodeReader to CanHandler";
    } else {
        qWarning() << "QrCodeReader not found in QML tree";
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [=] {
        hal_can_deinit();
        hal_deinit();
    });

    return app.exec();
}
