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
    , m_debugTimer(nullptr)
{
    qDebug() << "CanHandler: using interface" << m_interface;
    if (connectDevice()) {
        qDebug() << "CanHandler: device connected successfully";
    } else {
        qWarning() << "CanHandler: device connection FAILED at startup";
    }

    // Start cyclic debug CAN frame (1s interval)
    m_debugTimer = new QTimer(this);
    connect(m_debugTimer, &QTimer::timeout, this, &CanHandler::sendDebugFrame);
    m_debugTimer->start(1000);
    qDebug() << "CanHandler: debug timer started (1000ms), ID=0x18FF00FF";
}

CanHandler::~CanHandler()
{
    disconnectDevice();
}

bool CanHandler::connectDevice()
{
    if (m_socket >= 0)
        return true;

    qDebug() << "CanHandler: creating CAN socket...";
    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        qWarning() << "CanHandler: socket() failed:" << strerror(errno);
        return false;
    }
    qDebug() << "CanHandler: socket created (fd=" << m_socket << ")";

    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, m_interface.toLatin1().constData(), IFNAMSIZ - 1);
    qDebug() << "CanHandler: looking up interface index for" << m_interface;
    if (ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        qWarning() << "CanHandler: ioctl SIOCGIFINDEX failed:" << strerror(errno)
                    << "- is the interface UP?";
        close(m_socket);
        m_socket = -1;
        return false;
    }
    qDebug() << "CanHandler: interface index =" << ifr.ifr_ifindex;

    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    qDebug() << "CanHandler: binding socket to" << m_interface;
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

void CanHandler::sendDebugFrame()
{
    if (m_socket < 0 && !connectDevice()) {
        qWarning() << "CanHandler: debug frame skipped, device not connected";
        return;
    }

    struct can_frame frame = {};
    frame.can_id = CAN_ID_DEBUG | CAN_EFF_FLAG;
    frame.can_dlc = 8;
    memset(frame.data, 0xAA, 8);

    ssize_t nbytes = write(m_socket, &frame, sizeof(frame));
    if (nbytes != sizeof(frame)) {
        qWarning() << "CanHandler: debug frame write failed:" << strerror(errno);
    } else {
        qDebug() << "CanHandler: debug frame sent ID=0x18ff00ff";
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
