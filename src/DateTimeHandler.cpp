#include "DateTimeHandler.h"
#include <QDebug>

#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <linux/rtc.h>

extern "C" {
#include <libhal.h>
}

DateTimeHandler::DateTimeHandler(QObject *parent)
    : QObject(parent)
{
}

bool DateTimeHandler::setDateTime(int day, int month, int year, int hour, int minute)
{
    // Build rtc_time (month is 0-based, year is years since 1900)
    struct rtc_time t;
    memset(&t, 0, sizeof(t));
    t.tm_mday  = day;
    t.tm_mon   = month - 1;
    t.tm_year  = year - 1900;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = 0;

    hal_error err = hal_rtc_init();
    if (err != HAL_E_OK) {
        qWarning() << "DateTimeHandler: hal_rtc_init() failed:" << err;
        return false;
    }

    err = hal_rtc_set_time(&t);
    if (err != HAL_E_OK) {
        qWarning() << "DateTimeHandler: hal_rtc_set_time() failed:" << err;
        hal_rtc_deinit();
        return false;
    }

    hal_rtc_deinit();

    // Also sync the Linux system clock so new Date() in QML reflects the change
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_mday  = day;
    tm_val.tm_mon   = month - 1;
    tm_val.tm_year  = year - 1900;
    tm_val.tm_hour  = hour;
    tm_val.tm_min   = minute;
    tm_val.tm_sec   = 0;
    tm_val.tm_isdst = -1;

    time_t epoch = mktime(&tm_val);
    if (epoch == (time_t)-1) {
        qWarning() << "DateTimeHandler: mktime() failed";
        return false;
    }

    struct timeval tv;
    tv.tv_sec  = epoch;
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) {
        qWarning() << "DateTimeHandler: settimeofday() failed (need root?)";
        return false;
    }

    qDebug() << "DateTimeHandler: date/time set to"
             << QString("%1/%2/%3 %4:%5")
                .arg(day,   2, 10, QLatin1Char('0'))
                .arg(month, 2, 10, QLatin1Char('0'))
                .arg(year)
                .arg(hour,   2, 10, QLatin1Char('0'))
                .arg(minute, 2, 10, QLatin1Char('0'));
    return true;
}
