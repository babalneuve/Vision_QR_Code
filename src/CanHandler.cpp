#include "CanHandler.h"
#include <QDebug>

#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

CanHandler::CanHandler(const QString &interface, QObject *parent)
    : QObject(parent)
    , m_interface(interface)
    , m_socket(-1)
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
    if (m_socket >= 0)
        return true;

    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        qWarning() << "CanHandler: socket() failed:" << strerror(errno);
        return false;
    }

    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, m_interface.toLatin1().constData(), IFNAMSIZ - 1);
    if (ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        qWarning() << "CanHandler: ioctl SIOCGIFINDEX failed:" << strerror(errno);
        close(m_socket);
        m_socket = -1;
        return false;
    }

    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        qWarning() << "CanHandler: bind() failed:" << strerror(errno);
        close(m_socket);
        m_socket = -1;
        return false;
    }

    qDebug() << "CanHandler: connected to" << m_interface;
    return true;
}

void CanHandler::disconnectDevice()
{
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
        qDebug() << "CanHandler: disconnected from" << m_interface;
    }
}

void CanHandler::sendLedFrame(quint32 canId, bool state)
{
    if (m_socket < 0 && !connectDevice()) {
        qWarning() << "CanHandler: cannot send frame, device not connected";
        return;
    }

    struct can_frame frame = {};
    frame.can_id = canId | CAN_EFF_FLAG;
    frame.can_dlc = 8;
    memset(frame.data, 0xFF, 8);
    if (!state)
        frame.data[0] = 0xFE;  // bit 0 = 0, rest stays 1

    ssize_t nbytes = write(m_socket, &frame, sizeof(frame));
    if (nbytes != sizeof(frame)) {
        qWarning() << "CanHandler: write failed:" << strerror(errno);
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
