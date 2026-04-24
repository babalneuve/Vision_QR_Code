#include "UsbWatcher.h"
#include <QDebug>
#include <QMetaObject>
#include <QDir>
#include <QFile>

extern "C" {
#include <libhal.h>
}

// Single global instance pointer for the C-style HAL callbacks
static UsbWatcher *s_instance = nullptr;

UsbWatcher::UsbWatcher(QObject *parent)
    : QObject(parent)
    , m_usbPresent(false)
    , m_deviceCount(0)
{
    s_instance = this;

    // hal_udev_init() must be called before hal_blockdevice_init()
    if (!hal_udev_is_initialized()) {
        hal_error err = hal_udev_init();
        if (err != HAL_E_OK)
            qWarning() << "UsbWatcher: hal_udev_init() failed:" << err;
    }

    hal_error err = hal_blockdevice_init();
    if (err != HAL_E_OK) {
        qWarning() << "UsbWatcher: hal_blockdevice_init() failed:" << err;
        return;
    }

    hal_blockdevice_register_callback_device_add(&UsbWatcher::onDeviceAdded);
    hal_blockdevice_register_callback_device_remove(&UsbWatcher::onDeviceRemoved);

    // replay=true: replays already-connected devices so a USB plugged before
    // app start is detected immediately
    err = hal_udev_start_subsystem(true, HAL_UDEV_BLOCK_MASK);
    if (err != HAL_E_OK)
        qWarning() << "UsbWatcher: hal_udev_start_subsystem() failed:" << err;
}

UsbWatcher::~UsbWatcher()
{
    s_instance = nullptr;
}

bool UsbWatcher::usbPresent() const
{
    return m_usbPresent;
}

void UsbWatcher::setUsbPresent(bool present)
{
    if (present != m_usbPresent) {
        m_usbPresent = present;
        qDebug() << "UsbWatcher: USB" << (present ? "connected" : "removed");
        emit usbPresentChanged();
    }
}

void UsbWatcher::exportLogs(const QString &logDir)
{
    if (m_uuid.isEmpty()) {
        emit exportResult(false, tr("Aucune clé USB détectée."));
        return;
    }

    // --- 1. Resolve mount point ---
    char mountpoint[256] = {0};
    uint32_t mpSize = static_cast<uint32_t>(sizeof(mountpoint));

    hal_error err = hal_blockdevice_query(
        m_uuid.toLatin1().constData(),
        nullptr, nullptr,   // device_node — not needed
        nullptr, nullptr,   // fs_type    — not needed
        mountpoint, &mpSize
    );

    QString mp = QString::fromLatin1(mountpoint).trimmed();

    // If not already mounted, mount now
    if (mp.isEmpty()) {
        mpSize = static_cast<uint32_t>(sizeof(mountpoint));
        err = hal_blockdevice_mount2(m_uuid.toLatin1().constData(), mountpoint, &mpSize);
        if (err != HAL_E_OK) {
            qWarning() << "UsbWatcher: hal_blockdevice_mount2() failed:" << err;
            emit exportResult(false, tr("Impossible de monter la clé USB."));
            return;
        }
        mp = QString::fromLatin1(mountpoint).trimmed();
    }

    qDebug() << "UsbWatcher: export to mountpoint:" << mp;

    // --- 2. Collect log files ---
    QDir srcDir(logDir);
    if (!srcDir.exists()) {
        emit exportResult(false, tr("Dossier de logs introuvable."));
        return;
    }

    const QStringList files = srcDir.entryList(QDir::Files);
    if (files.isEmpty()) {
        emit exportResult(false, tr("Aucun log à exporter."));
        return;
    }

    // --- 3. Create destination folder <USB>/Logs/ ---
    QDir destDir(mp + QStringLiteral("/Logs"));
    if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
        emit exportResult(false, tr("Impossible de créer le dossier Logs sur la clé USB."));
        return;
    }

    // --- 4. Copy each file ---
    for (const QString &fileName : files) {
        const QString src = srcDir.filePath(fileName);
        const QString dst = destDir.filePath(fileName);

        // Overwrite if already exists on USB
        if (QFile::exists(dst))
            QFile::remove(dst);

        if (!QFile::copy(src, dst)) {
            qWarning() << "UsbWatcher: copy failed:" << src << "->" << dst;
            emit exportResult(false, tr("Erreur lors de la copie de %1.").arg(fileName));
            return;
        }
    }

    // --- 5. Delete logs from internal storage ---
    for (const QString &fileName : files)
        QFile::remove(srcDir.filePath(fileName));

    qDebug() << "UsbWatcher: export complete," << files.size() << "file(s) exported";
    emit exportResult(true, tr("%1 fichier(s) exporté(s) avec succès.").arg(files.size()));
}

// Called from the HAL thread — use QueuedConnection to safely reach the Qt main thread
void UsbWatcher::onDeviceAdded(char const * const uuid)
{
    qDebug() << "UsbWatcher: block device added, uuid =" << uuid;
    if (!s_instance)
        return;

    s_instance->m_deviceCount++;
    s_instance->m_uuid = QString::fromLatin1(uuid);

    QMetaObject::invokeMethod(s_instance, [=]() {
        if (s_instance)
            s_instance->setUsbPresent(true);
    }, Qt::QueuedConnection);
}

void UsbWatcher::onDeviceRemoved(char const * const uuid)
{
    qDebug() << "UsbWatcher: block device removed, uuid =" << uuid;
    if (!s_instance)
        return;

    if (s_instance->m_deviceCount > 0)
        s_instance->m_deviceCount--;

    if (s_instance->m_deviceCount == 0)
        s_instance->m_uuid.clear();

    QMetaObject::invokeMethod(s_instance, [=]() {
        if (s_instance && s_instance->m_deviceCount == 0)
            s_instance->setUsbPresent(false);
    }, Qt::QueuedConnection);
}
