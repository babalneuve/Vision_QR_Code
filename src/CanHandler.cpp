#include "CanHandler.h"
#include <QDebug>

CanHandler::CanHandler(const QString &interface, QObject *parent)
    : QObject(parent)
    , m_interface(interface)
    , m_device(nullptr)
{
    qDebug() << "CanHandler: using interface" << m_interface;
    connectDevice();
}

CanHandler::~CanHandler()
{
    disconnectDevice();
}

bool CanHandler::connectDevice()
{
    if (m_device)
        return true;

    QString errorString;
    m_device = QCanBus::instance()->createDevice(
        QStringLiteral("socketcan"), m_interface, &errorString);

    if (!m_device) {
        qWarning() << "CanHandler: failed to create device:" << errorString;
        return false;
    }

    if (!m_device->connectDevice()) {
        qWarning() << "CanHandler: failed to connect:" << m_device->errorString();
        delete m_device;
        m_device = nullptr;
        return false;
    }

    qDebug() << "CanHandler: connected to" << m_interface;
    return true;
}

void CanHandler::disconnectDevice()
{
    if (m_device) {
        m_device->disconnectDevice();
        delete m_device;
        m_device = nullptr;
        qDebug() << "CanHandler: disconnected from" << m_interface;
    }
}

void CanHandler::sendLedFrame(quint32 canId, bool state)
{
    if (!m_device && !connectDevice()) {
        qWarning() << "CanHandler: cannot send frame, device not connected";
        return;
    }

    // DLC=8, default byte 0xFF, LED state is bit 0 of byte 0
    QByteArray payload(8, static_cast<char>(0xFF));
    if (state)
        payload[0] = static_cast<char>(0xFF);  // bit 0 = 1 (all default bits stay 0xFF)
    else
        payload[0] = static_cast<char>(0xFE);  // bit 0 = 0, rest stays 1

    QCanBusFrame frame(canId, payload);
    frame.setExtendedFrameFormat(true);

    if (!m_device->writeFrame(frame)) {
        qWarning() << "CanHandler: writeFrame failed:" << m_device->errorString();
    } else {
        qDebug() << "CanHandler: sent extended frame ID="
                 << QString("0x%1").arg(canId, 8, 16, QLatin1Char('0'))
                 << "state=" << state;
    }
}

void CanHandler::onQrCodeDetected(const QString &data)
{
    if (data == QStringLiteral("LED 1 : ON")) {
        qDebug() << "CanHandler: QR code 'LED 1 : ON'";
        sendLedFrame(CAN_ID_LED1, true);
    } else if (data == QStringLiteral("LED 1 : OFF")) {
        qDebug() << "CanHandler: QR code 'LED 1 : OFF'";
        sendLedFrame(CAN_ID_LED1, false);
    } else if (data == QStringLiteral("LED 2 : ON")) {
        qDebug() << "CanHandler: QR code 'LED 2 : ON'";
        sendLedFrame(CAN_ID_LED2, true);
    } else if (data == QStringLiteral("LED 2 : OFF")) {
        qDebug() << "CanHandler: QR code 'LED 2 : OFF'";
        sendLedFrame(CAN_ID_LED2, false);
    } else {
        qDebug() << "CanHandler: ignoring unrecognized QR code:" << data;
    }
}
