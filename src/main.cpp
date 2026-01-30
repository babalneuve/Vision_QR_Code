#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include <QQmlContext>
#include <QDir>
#include <QPluginLoader>

QObject * findByClassName(const QObject * const o, const char *name) {
  QObject * res = nullptr;
  foreach (QObject * c, o->children()) {
//    qDebug().noquote() << "NAME FOUND " << c->metaObject()->className();
    if (res) break;
    if (QLatin1String(c->metaObject()->className()) == name) res = c;
    else res = findByClassName(c, name);
  }
  return res;
}

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
    QQmlApplicationEngine engine;


    const QUrl url(QStringLiteral("qrc:/main.qml"));

    engine.load(url);

    QObject *root = qobject_cast<QObject*>(engine.rootObjects().first());

    QObject *button1 = root->findChild<QObject*>("ip_button1", Qt::FindChildrenRecursively);

    QObject *button2 = root->findChild<QObject*>("ip_button2", Qt::FindChildrenRecursively);

    QObject *resetButton1 = root->findChild<QObject*>("reset_pipeline_button1", Qt::FindChildrenRecursively);

    QObject *stopButton1 = root->findChild<QObject*>("stop_button1", Qt::FindChildrenRecursively);

    QObject *resetButton2 = root->findChild<QObject*>("reset_pipeline_button2", Qt::FindChildrenRecursively);

    QObject *stopButton2 = root->findChild<QObject*>("stop_button2", Qt::FindChildrenRecursively);

    cam1 = engine.rootObjects().at(0)->findChild<QObject *>("camera1",  Qt::FindChildrenRecursively)->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);

    cam2 = engine.rootObjects().at(0)->findChild<QObject *>("camera2",  Qt::FindChildrenRecursively)->findChild<QObject *>("DigitalCameraController", Qt::FindChildrenRecursively);

    QObject::connect(button1, SIGNAL(ipChange1(QString)), cam1, SLOT(changeIP(QString)));

    QObject::connect(button2, SIGNAL(ipChange2(QString)), cam2, SLOT(changeIP(QString)));

    QObject::connect(resetButton1, SIGNAL(resetPipeline1()), cam1, SLOT(resetPipeline()));

    QObject::connect(stopButton1, SIGNAL(stopPipeline1()), cam1, SLOT(stopPipeline()));

    QObject::connect(resetButton2, SIGNAL(resetPipeline2()), cam2, SLOT(resetPipeline()));

    QObject::connect(stopButton2, SIGNAL(stopPipeline2()), cam2, SLOT(stopPipeline()));

    return app.exec();
}
