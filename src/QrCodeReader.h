/**
 * @file QrCodeReader.h
 * @brief Qt wrapper for quirc QR code recognition library
 *
 * Provides QML-accessible QR code detection and decoding functionality
 * for use with HmiDigitalCamera video feeds.
 */

#ifndef QRCODEREADER_H
#define QRCODEREADER_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QString>
#include <QVideoProbe>
#include <QVideoFrame>

// Forward declaration
struct quirc;

/**
 * @class QrCodeReader
 * @brief QML-accessible QR code reader using the quirc library
 *
 * This class provides continuous QR code scanning from camera feeds.
 * It uses Item.grabToImage() to capture frames from QML components
 * and processes them for QR codes.
 */
class QrCodeReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning WRITE setScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanInterval READ scanInterval WRITE setScanInterval NOTIFY scanIntervalChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY qrCodeDetected)
    Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit QrCodeReader(QObject *parent = nullptr);

    /**
     * @brief Destructor - cleans up quirc resources
     */
    ~QrCodeReader() override;

    /**
     * @brief Check if scanning is active
     * @return true if continuous scanning is enabled
     */
    bool isScanning() const;

    /**
     * @brief Set scanning state
     * @param enabled Enable or disable continuous scanning
     */
    void setScanning(bool enabled);

    /**
     * @brief Get current scan interval in milliseconds
     * @return Scan interval in ms
     */
    int scanInterval() const;

    /**
     * @brief Set scan interval
     * @param interval Interval in milliseconds (minimum 100ms)
     */
    void setScanInterval(int interval);

    /**
     * @brief Get the last detected QR code result
     * @return Last decoded QR code content
     */
    QString lastResult() const;

    /**
     * @brief Get the target QML item for frame capture
     * @return Pointer to target QQuickItem
     */
    QQuickItem* target() const;

    /**
     * @brief Set the target QML item for frame capture
     * @param item The QQuickItem to capture frames from
     */
    void setTarget(QQuickItem* item);

    /**
     * @brief Get the source camera object for QVideoProbe
     * @return Pointer to source QObject (Camera)
     */
    QObject* source() const;

    /**
     * @brief Set the source camera object for QVideoProbe
     * @param obj The Camera QML element to probe frames from
     */
    void setSource(QObject* obj);

    /**
     * @brief Decode QR code from a QImage
     * @param image The image to scan for QR codes
     * @return Decoded QR code data, or empty string if none found
     *
     * This method is Q_INVOKABLE for direct use from QML.
     */
    Q_INVOKABLE QString decode(const QImage &image);

    /**
     * @brief Manually trigger a single scan
     *
     * Captures a frame from the target and attempts to decode any QR codes.
     */
    Q_INVOKABLE void scanOnce();

signals:
    /**
     * @brief Emitted when a QR code is detected
     * @param data The decoded QR code content
     */
    void qrCodeDetected(const QString &data);

    /**
     * @brief Emitted when scanning state changes
     */
    void scanningChanged();

    /**
     * @brief Emitted when scan interval changes
     */
    void scanIntervalChanged();

    /**
     * @brief Emitted when target item changes
     */
    void targetChanged();

    /**
     * @brief Emitted when source camera changes
     */
    void sourceChanged();

    /**
     * @brief Emitted when a scan completes (for debugging)
     * @param found Whether a QR code was found
     * @param timeMs Time taken for the scan in milliseconds
     */
    void scanComplete(bool found, int timeMs);

private slots:
    /**
     * @brief Called by timer to perform periodic scanning
     */
    void performScan();

    /**
     * @brief Called when grabToImage completes
     */
    void onGrabComplete();

    /**
     * @brief Called when QVideoProbe receives a frame
     */
    void onVideoFrame(const QVideoFrame &frame);

    /**
     * @brief Retry probe attachment when camera status changes
     */
    void tryAttachProbe();

private:
    /**
     * @brief Attempt to attach QVideoProbe to source's mediaObject
     * @return true if probe was successfully attached
     */
    bool attachProbe();
    /**
     * @brief Convert QImage to grayscale for quirc processing
     * @param image Input image (any format)
     * @return Grayscale image in Format_Grayscale8
     */
    QImage toGrayscale(const QImage &image);

    /**
     * @brief Process an image and decode QR codes
     * @param grayscale Grayscale image to process
     * @return Decoded data or empty string
     */
    QString processImage(const QImage &grayscale);

    struct quirc *m_quirc;          ///< quirc decoder instance
    QTimer *m_scanTimer;            ///< Timer for periodic scanning
    bool m_scanning;                ///< Current scanning state
    int m_scanInterval;             ///< Scan interval in ms
    QString m_lastResult;           ///< Last detected QR code
    QQuickItem *m_target;           ///< Target item for frame capture
    QSharedPointer<QQuickItemGrabResult> m_grabResult; ///< Prevents grab result from being destroyed before ready signal
    QString m_pendingResult;        ///< Result from last debounce check
    int m_debounceCount;            ///< Count of same results for debouncing
    QObject *m_source;              ///< Source camera object for QVideoProbe
    QVideoProbe *m_videoProbe;      ///< Probes raw frames from camera pipeline
    QElapsedTimer m_frameClock;     ///< Throttle probe frame processing
};

#endif // QRCODEREADER_H
