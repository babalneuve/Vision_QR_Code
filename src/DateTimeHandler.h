#ifndef DATETIMEHANDLER_H
#define DATETIMEHANDLER_H

#include <QObject>

class DateTimeHandler : public QObject
{
    Q_OBJECT

public:
    explicit DateTimeHandler(QObject *parent = nullptr);

    /** Set date and time on both the RTC and the system clock.
     *  month: 1-12, year: full (e.g. 2025)
     *  Returns true on success. */
    Q_INVOKABLE bool setDateTime(int day, int month, int year, int hour, int minute);
};

#endif // DATETIMEHANDLER_H
