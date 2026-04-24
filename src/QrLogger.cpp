#include "QrLogger.h"
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>

QrLogger::QrLogger(const QString &logDir, QObject *parent)
    : QObject(parent)
    , m_logDir(logDir)
    , m_led1State(QStringLiteral("?"))
    , m_led2State(QStringLiteral("?"))
    , m_lastEntryValid(-1)
{
    QDir().mkpath(m_logDir);
    openLogFile(QDate::currentDate());
}

QrLogger::~QrLogger()
{
    if (m_logFile.isOpen())
        m_logFile.close();
}

void QrLogger::openLogFile(const QDate &date)
{
    if (m_logFile.isOpen())
        m_logFile.close();

    m_currentDate = date;

    const QString fileName = m_logDir + QDir::separator()
                             + date.toString("yyyy-MM-dd") + ".log";
    m_logFile.setFileName(fileName);

    const bool existed = QFile::exists(fileName);

    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "QrLogger: cannot open log file:" << fileName;
        return;
    }

    if (!existed) {
        // Write a header on first creation
        QTextStream out(&m_logFile);
        out << "=== QR Code log — " << date.toString("dd/MM/yyyy") << " ===\n";
    }

    qDebug() << "QrLogger:" << (existed ? "appending to" : "created") << fileName;
}

bool QrLogger::isValidCommand(const QString &data) const
{
    const QStringList lines = data.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString cmd = line.trimmed();
        if (cmd == "LED 1 : ON"  || cmd == "LED 1 : OFF" ||
            cmd == "LED 2 : ON"  || cmd == "LED 2 : OFF")
            return true;
    }
    return false;
}


bool QrLogger::applyCommands(const QString &data)
{
    bool matched = false;
    const QStringList lines = data.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString cmd = line.trimmed();
        if (cmd == "LED 1 : ON")       { m_led1State = "ON";  matched = true; }
        else if (cmd == "LED 1 : OFF") { m_led1State = "OFF"; matched = true; }
        else if (cmd == "LED 2 : ON")  { m_led2State = "ON";  matched = true; }
        else if (cmd == "LED 2 : OFF") { m_led2State = "OFF"; matched = true; }
    }
    return matched;
}

void QrLogger::logScan(const QString &data)
{
    // Re-open if the date changed (midnight or manual date change)
    const QDate today = QDate::currentDate();
    if (today != m_currentDate)
        openLogFile(today);

    if (!m_logFile.isOpen()) {
        qWarning() << "QrLogger: log file not open, skipping entry";
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

    QString entry;
    const bool valid = applyCommands(data);
    const bool typeChanged = (m_lastEntryValid != -1) && (m_lastEntryValid != (int)valid);
    const QString separator = typeChanged ? "\n" : "";

    if (valid) {
        entry = QString("%1[%2] [VALIDE]   LED 1 : %3 | LED 2 : %4\n")
                .arg(separator, timestamp, m_led1State, m_led2State);
    } else {
        entry = QString("%1[%2] [INVALIDE] %3\n")
                .arg(separator, timestamp, data.trimmed());
    }

    m_lastEntryValid = (int)valid;

    QTextStream out(&m_logFile);
    out << entry;
    m_logFile.flush();

    qDebug() << "QrLogger: logged" << entry.trimmed();
}
