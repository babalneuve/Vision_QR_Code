#ifndef QRLOGGER_H
#define QRLOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QDate>

/**
 * @class QrLogger
 * @brief Logs every QR code scan to a daily file (YYYY-MM-DD.log).
 *
 * On each scan the current date is checked. If it changed since the last
 * write (midnight or manual date change), the logger looks for an existing
 * file for the new date and appends to it, or creates a new one.
 */
class QrLogger : public QObject
{
    Q_OBJECT

public:
    explicit QrLogger(const QString &logDir, QObject *parent = nullptr);
    ~QrLogger() override;

public slots:
    /** Called for every detected QR code (valid or invalid). */
    void logScan(const QString &data);

private:
    /** Open (or create) the log file for the given date. */
    void openLogFile(const QDate &date);

    /** Return true if data contains at least one recognized LED command. */
    bool isValidCommand(const QString &data) const;

    /** Update LED states from the command lines and return true if any matched. */
    bool applyCommands(const QString &data);

    QString m_logDir;
    QFile   m_logFile;
    QDate   m_currentDate;

    QString m_led1State;   ///< "ON", "OFF" or "?" (unknown)
    QString m_led2State;
    int     m_lastEntryValid; ///< 1=valid, 0=invalid, -1=none yet
};

#endif // QRLOGGER_H
