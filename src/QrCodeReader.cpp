/**
 * @file QrCodeReader.cpp
 * @brief Implementation of QrCodeReader class
 */

#include "QrCodeReader.h"
#include "quirc/quirc.h"

#include <QDebug>
#include <QQuickItemGrabResult>
#include <QElapsedTimer>

// Minimum scan interval to prevent CPU overload
static const int MIN_SCAN_INTERVAL = 100;
// Default scan interval (300ms = ~3 scans per second)
static const int DEFAULT_SCAN_INTERVAL = 300;
// Number of consecutive identical results required before emitting
static const int DEBOUNCE_COUNT = 2;

QrCodeReader::QrCodeReader(QObject *parent)
    : QObject(parent)
    , m_quirc(nullptr)
    , m_scanTimer(nullptr)
    , m_scanning(false)
    , m_scanInterval(DEFAULT_SCAN_INTERVAL)
    , m_target(nullptr)
    , m_debounceCount(0)
{
    // Initialize quirc decoder
    m_quirc = quirc_new();
    if (!m_quirc) {
        qWarning() << "QrCodeReader: Failed to create quirc decoder";
    }

    // Setup scan timer
    m_scanTimer = new QTimer(this);
    m_scanTimer->setInterval(m_scanInterval);
    connect(m_scanTimer, &QTimer::timeout, this, &QrCodeReader::performScan);
}

QrCodeReader::~QrCodeReader()
{
    if (m_quirc) {
        quirc_destroy(m_quirc);
        m_quirc = nullptr;
    }
}

bool QrCodeReader::isScanning() const
{
    return m_scanning;
}

void QrCodeReader::setScanning(bool enabled)
{
    if (m_scanning == enabled) {
        return;
    }

    m_scanning = enabled;

    if (m_scanning) {
        if (m_target) {
            m_scanTimer->start();
            qDebug() << "QrCodeReader: Started scanning with interval" << m_scanInterval << "ms";
        } else {
            qWarning() << "QrCodeReader: Cannot start scanning - no target set";
            m_scanning = false;
        }
    } else {
        m_scanTimer->stop();
        qDebug() << "QrCodeReader: Stopped scanning";
    }

    emit scanningChanged();
}

int QrCodeReader::scanInterval() const
{
    return m_scanInterval;
}

void QrCodeReader::setScanInterval(int interval)
{
    // Clamp to minimum interval
    if (interval < MIN_SCAN_INTERVAL) {
        interval = MIN_SCAN_INTERVAL;
    }

    if (m_scanInterval == interval) {
        return;
    }

    m_scanInterval = interval;
    m_scanTimer->setInterval(m_scanInterval);
    emit scanIntervalChanged();
}

QString QrCodeReader::lastResult() const
{
    return m_lastResult;
}

QQuickItem* QrCodeReader::target() const
{
    return m_target;
}

void QrCodeReader::setTarget(QQuickItem* item)
{
    if (m_target == item) {
        return;
    }

    m_target = item;
    emit targetChanged();
}

QString QrCodeReader::decode(const QImage &image)
{
    if (!m_quirc) {
        qWarning() << "QrCodeReader: Decoder not initialized";
        return QString();
    }

    if (image.isNull()) {
        return QString();
    }

    // Convert to grayscale
    QImage grayscale = toGrayscale(image);

    // Process the image
    return processImage(grayscale);
}

void QrCodeReader::scanOnce()
{
    if (!m_target) {
        qWarning() << "QrCodeReader: No target set for scanning";
        return;
    }

    performScan();
}

void QrCodeReader::performScan()
{
    if (!m_target || !m_quirc) {
        return;
    }

    // Use grabToImage to capture the current frame
    QSharedPointer<QQuickItemGrabResult> result = m_target->grabToImage();
    if (result) {
        connect(result.data(), &QQuickItemGrabResult::ready,
                this, &QrCodeReader::onGrabComplete);
    }
}

void QrCodeReader::onGrabComplete()
{
    QQuickItemGrabResult *result = qobject_cast<QQuickItemGrabResult*>(sender());
    if (!result) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QImage image = result->image();

    QString decoded = decode(image);

    int elapsed = timer.elapsed();
    bool found = !decoded.isEmpty();

    emit scanComplete(found, elapsed);

    if (found) {
        // Debouncing: require multiple consecutive identical detections
        if (decoded == m_pendingResult) {
            m_debounceCount++;
        } else {
            m_pendingResult = decoded;
            m_debounceCount = 1;
        }

        // Only emit if we've seen the same result multiple times
        // AND it's different from the last reported result
        if (m_debounceCount >= DEBOUNCE_COUNT && decoded != m_lastResult) {
            m_lastResult = decoded;
            qDebug() << "QrCodeReader: Detected QR code:" << decoded;
            emit qrCodeDetected(decoded);
        }
    } else {
        // Reset debounce state when no QR code is visible
        m_pendingResult.clear();
        m_debounceCount = 0;
    }
}

QImage QrCodeReader::toGrayscale(const QImage &image)
{
    // If already grayscale, return as-is
    if (image.format() == QImage::Format_Grayscale8) {
        return image;
    }

    // Convert to grayscale
    return image.convertToFormat(QImage::Format_Grayscale8);
}

QString QrCodeReader::processImage(const QImage &grayscale)
{
    if (!m_quirc || grayscale.isNull()) {
        return QString();
    }

    int width = grayscale.width();
    int height = grayscale.height();

    // Resize quirc buffer if needed
    if (quirc_resize(m_quirc, width, height) < 0) {
        qWarning() << "QrCodeReader: Failed to resize quirc buffer";
        return QString();
    }

    // Get pointer to quirc image buffer
    int w, h;
    uint8_t *buffer = quirc_begin(m_quirc, &w, &h);

    // Copy image data to quirc buffer
    // Handle case where image stride may differ from width
    for (int y = 0; y < height && y < h; y++) {
        const uchar *scanLine = grayscale.constScanLine(y);
        memcpy(buffer + y * w, scanLine, qMin(width, w));
    }

    // Process the image
    quirc_end(m_quirc);

    // Check for detected QR codes
    int count = quirc_count(m_quirc);
    if (count == 0) {
        return QString();
    }

    // Try to decode each detected QR code
    for (int i = 0; i < count; i++) {
        struct quirc_code code;
        struct quirc_data data;

        quirc_extract(m_quirc, i, &code);
        quirc_decode_error_t err = quirc_decode(&code, &data);

        if (err == QUIRC_SUCCESS) {
            // Successfully decoded - return the payload
            return QString::fromUtf8(reinterpret_cast<const char*>(data.payload),
                                     data.payload_len);
        }

        // Try flipping if first decode failed
        quirc_flip(&code);
        err = quirc_decode(&code, &data);
        if (err == QUIRC_SUCCESS) {
            return QString::fromUtf8(reinterpret_cast<const char*>(data.payload),
                                     data.payload_len);
        }
    }

    return QString();
}
