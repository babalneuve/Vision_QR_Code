#ifndef CANHANDLER_H
#define CANHANDLER_H

#include <QObject>
#include <QString>

class CanHandler : public QObject
{
    Q_OBJECT

public:
    explicit CanHandler(const QString &interface = QStringLiteral("can0"),
                        QObject *parent = nullptr);
    ~CanHandler() override;

public slots:
    void onQrCodeDetected(const QString &data);

private:
    bool connectDevice();
    void disconnectDevice();
    void sendLedFrame(quint32 canId, bool state);

    static const quint32 CAN_ID_LED1 = 0x18FF0000;
    static const quint32 CAN_ID_LED2 = 0x18FF0001;

    QString m_interface;
    int m_socket;
};

#endif // CANHANDLER_H
