/**
 * @file QrCodeReader.cpp
 * @brief Implementation of QrCodeReader class
 */

#include "QrCodeReader.h"
#include "quirc/quirc.h"

#include <QDebug>
#include <QQuickItemGrabResult>
#include <QElapsedTimer>
#include <QMediaObject>

// Minimum scan interval to prevent CPU overload
static const int MIN_SCAN_INTERVAL = 100;
// Default scan interval (300ms = ~3 scans per second)
static const int DEFAULT_SCAN_INTERVAL = 300;
// Number of consecutive identical results required before emitting
static const int DEBOUNCE_COUNT = 2;
// Maximum image dimension for quirc processing — limits CPU usage per frame.
static const int QUIRC_MAX_DIM = 640;

QrCodeReader::QrCodeReader(QObject *parent)
    : QObject(parent)
    , m_quirc(nullptr)
    , m_scanTimer(nullptr)
    , m_scanning(false)
    , m_scanInterval(DEFAULT_SCAN_INTERVAL)
    , m_target(nullptr)
    , m_debounceCount(0)
    , m_source(nullptr)
    , m_videoProbe(nullptr)
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
        if (m_videoProbe) {
            // QVideoProbe path: probe fires on every frame, throttled by m_frameClock
            m_frameClock.start();
            qDebug() << "QrCodeReader: Started scanning via QVideoProbe with interval" << m_scanInterval << "ms";
        } else if (m_source) {
            // Source set but probe not yet attached (camera still loading)
            // Scanning flag is set; tryAttachProbe() will start the clock
            qDebug() << "QrCodeReader: Scanning enabled, waiting for camera probe attachment";
        } else if (m_target) {
            // Fallback: grabToImage path (embedded build)
            m_scanTimer->start();
            qDebug() << "QrCodeReader: Started scanning via grabToImage with interval" << m_scanInterval << "ms";
        } else {
            qWarning() << "QrCodeReader: Cannot start scanning - no source or target set";
            m_scanning = false;
        }
    } else {
        m_scanTimer->stop();
        // Reset debounce state so re-scanning can detect the same code again
        m_lastResult.clear();
        m_pendingResult.clear();
        m_debounceCount = 0;
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

QObject* QrCodeReader::source() const
{
    return m_source;
}

void QrCodeReader::setSource(QObject* obj)
{
    if (m_source == obj) {
        return;
    }

    // Disconnect from previous source's status changes
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }

    m_source = obj;

    // Clean up old probe
    if (m_videoProbe) {
        delete m_videoProbe;
        m_videoProbe = nullptr;
    }

    if (m_source) {
        // Try to attach probe now; if mediaObject isn't ready yet,
        // retry when camera status changes
        if (!attachProbe()) {
            connect(m_source, SIGNAL(cameraStatusChanged()),
                    this, SLOT(tryAttachProbe()));
        }
    }

    emit sourceChanged();
}

bool QrCodeReader::attachProbe()
{
    if (!m_source || m_videoProbe) {
        return m_videoProbe != nullptr;
    }

    QVariant moVar = m_source->property("mediaObject");
    QMediaObject *mediaObject = qvariant_cast<QMediaObject*>(moVar);

    if (!mediaObject) {
        return false;
    }

    m_videoProbe = new QVideoProbe(this);
    if (m_videoProbe->setSource(mediaObject)) {
        connect(m_videoProbe, &QVideoProbe::videoFrameProbed,
                this, &QrCodeReader::onVideoFrame);
        qDebug() << "QrCodeReader: QVideoProbe attached to camera";
        return true;
    }

    qWarning() << "QrCodeReader: Failed to attach QVideoProbe to media object";
    delete m_videoProbe;
    m_videoProbe = nullptr;
    return false;
}

void QrCodeReader::tryAttachProbe()
{
    if (attachProbe()) {
        // Successfully attached — stop listening for status changes
        disconnect(m_source, SIGNAL(cameraStatusChanged()),
                   this, SLOT(tryAttachProbe()));

        // If scanning was requested before probe was ready, start the clock
        if (m_scanning) {
            m_frameClock.start();
            qDebug() << "QrCodeReader: Probe now ready, scanning active";
        }
    }
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

    // Downscale if too large — quirc's line_intersect() overflows int on large images
    if (grayscale.width() > QUIRC_MAX_DIM || grayscale.height() > QUIRC_MAX_DIM) {
        grayscale = grayscale.scaled(QUIRC_MAX_DIM, QUIRC_MAX_DIM,
                                     Qt::KeepAspectRatio, Qt::FastTransformation);
    }

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

    // Skip if a previous grab is still in-flight
    if (m_grabResult) {
        return;
    }

    // Store in member to prevent destruction before ready signal fires
    m_grabResult = m_target->grabToImage();
    if (!m_grabResult) {
        qWarning() << "QrCodeReader: grabToImage() returned null";
    } else {
        connect(m_grabResult.data(), &QQuickItemGrabResult::ready,
                this, &QrCodeReader::onGrabComplete);
    }
}

void QrCodeReader::onGrabComplete()
{
    QQuickItemGrabResult *result = qobject_cast<QQuickItemGrabResult*>(sender());
    if (!result) {
        m_grabResult.reset();
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QImage image = result->image();

    // Release the grab result now that we have the image
    m_grabResult.reset();

    if (image.isNull()) {
        qWarning() << "QrCodeReader: grabbed image is null";
        return;
    }

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

void QrCodeReader::onVideoFrame(const QVideoFrame &frame)
{
    if (!m_scanning || !m_quirc) {
        return;
    }

    // Throttle: skip frames until scan interval has elapsed
    if (m_frameClock.isValid() && m_frameClock.elapsed() < m_scanInterval) {
        return;
    }
    m_frameClock.restart();

    // Map frame to CPU memory
    QVideoFrame f(frame);
    if (!f.map(QAbstractVideoBuffer::ReadOnly)) {
        return;
    }

    QImage image;
    // Convert QVideoFrame to QImage based on pixel format
    switch (f.pixelFormat()) {
    case QVideoFrame::Format_YUYV:
    case QVideoFrame::Format_UYVY: {
        // For YUYV/UYVY, extract Y channel directly (every other byte starting at 0 or 1)
        int w = f.width();
        int h = f.height();
        image = QImage(w, h, QImage::Format_Grayscale8);
        int yOffset = (f.pixelFormat() == QVideoFrame::Format_YUYV) ? 0 : 1;
        for (int y = 0; y < h; y++) {
            const uchar *src = f.bits() + y * f.bytesPerLine();
            uchar *dst = image.scanLine(y);
            for (int x = 0; x < w; x++) {
                dst[x] = src[x * 2 + yOffset];
            }
        }
        break;
    }
    case QVideoFrame::Format_NV12:
    case QVideoFrame::Format_NV21:
    case QVideoFrame::Format_YUV420P:
    case QVideoFrame::Format_YV12: {
        // Y plane is the first plane; stride = bytesPerLine, width bytes of luma per row
        int w = f.width();
        int h = f.height();
        image = QImage(w, h, QImage::Format_Grayscale8);
        for (int y = 0; y < h; y++) {
            memcpy(image.scanLine(y), f.bits() + y * f.bytesPerLine(), w);
        }
        break;
    }
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_ARGB32:
        image = QImage(f.bits(), f.width(), f.height(), f.bytesPerLine(),
                       QImage::Format_ARGB32).copy();
        break;
    case QVideoFrame::Format_BGR32:
        image = QImage(f.bits(), f.width(), f.height(), f.bytesPerLine(),
                       QImage::Format_RGB32).copy();
        break;
    default:
        // Unknown format — attempt generic conversion via QImage
        if (QVideoFrame::imageFormatFromPixelFormat(f.pixelFormat()) != QImage::Format_Invalid) {
            image = QImage(f.bits(), f.width(), f.height(), f.bytesPerLine(),
                           QVideoFrame::imageFormatFromPixelFormat(f.pixelFormat())).copy();
        } else {
            f.unmap();
            return;
        }
        break;
    }

    f.unmap();

    if (image.isNull()) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QString decoded = decode(image);

    int elapsed = timer.elapsed();
    bool found = !decoded.isEmpty();

    emit scanComplete(found, elapsed);

    if (found) {
        if (decoded == m_pendingResult) {
            m_debounceCount++;
        } else {
            m_pendingResult = decoded;
            m_debounceCount = 1;
        }

        if (m_debounceCount >= DEBOUNCE_COUNT && decoded != m_lastResult) {
            m_lastResult = decoded;
            qDebug() << "QrCodeReader: Detected QR code:" << decoded;
            emit qrCodeDetected(decoded);
        }
    } else {
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
