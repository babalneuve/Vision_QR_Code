#ifndef USBWATCHER_H
#define USBWATCHER_H

#include <QObject>
#include <QString>

/**
 * @class UsbWatcher
 * @brief Detects USB mass storage connection/disconnection via the Vision 3 HAL
 *        block device API (hal_blockdevice / hal_udev).
 *
 * Also provides exportLogs() to copy log files to the USB drive and delete them
 * from the screen's internal storage.
 *
 * Exposes a usbPresent property for QML binding.
 */
class UsbWatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool usbPresent READ usbPresent NOTIFY usbPresentChanged)

public:
    explicit UsbWatcher(QObject *parent = nullptr);
    ~UsbWatcher() override;

    bool usbPresent() const;

    /**
     * @brief Copy all files from logDir to <USB mountpoint>/Logs/, then delete
     *        them from logDir. Emits exportResult() when done.
     * @param logDir Absolute path to the log directory on internal storage.
     */
    Q_INVOKABLE void exportLogs(const QString &logDir);

signals:
    void usbPresentChanged();

    /**
     * @brief Emitted after exportLogs() completes.
     * @param success true if all files were copied and deleted successfully.
     * @param message Human-readable status message.
     */
    void exportResult(bool success, const QString &message);

private:
    // Static C-style callbacks required by the HAL API
    static void onDeviceAdded(char const * const uuid);
    static void onDeviceRemoved(char const * const uuid);

    void setUsbPresent(bool present);

    bool    m_usbPresent;
    int     m_deviceCount;   // number of block devices currently connected
    QString m_uuid;          // UUID of the last connected block device
};

#endif // USBWATCHER_H
