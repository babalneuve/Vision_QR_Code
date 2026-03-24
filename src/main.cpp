#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include <QLoggingCategory>
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
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_NativeWindows);

    QGuiApplication app(argc, argv);

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

    // Plus besoin de rechercher les boutons supprimés
    // On ne garde que la caméra 1
    QObject *camera1 = root->findChild<QObject *>("camera1", Qt::FindChildrenRecursively);

    if (camera1)
        cam1 = camera1->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);

    if (!cam1) {
        qCritical() << "Failed to find camera 1 controller";
        return -1;
    }

    // Initialiser le GStreamer bus pour la caméra 1 uniquement
    QMetaObject::invokeMethod(cam1, "GetGstBus");

    // Toutes les connexions aux boutons supprimés sont retirées
    // On ne garde que ce qui est nécessaire pour la caméra 1 si besoin

    // Note: Les boutons de contrôle (IP, reset, stop) ont été supprimés de l'interface
    // Donc plus besoin de connexions pour ces fonctionnalités

    return app.exec();
}
