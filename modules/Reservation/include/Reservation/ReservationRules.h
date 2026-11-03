#pragma once

// 成员 B：供 SeatManager 和 AttendanceViolationManager 共同使用的时间及状态规则。
#include "Models.h"
#include "Reservation/Export.h"

#include <QDateTime>
#include <QString>
#include <QVector>

RESERVATION_API QDateTime toMinute(const QDateTime &value);
RESERVATION_API bool isHalfHourAligned(const QDateTime &value);
RESERVATION_API bool overlaps(const QDateTime &startA, const QDateTime &endA,
                           const QDateTime &startB, const QDateTime &endB);
RESERVATION_API ReservationViewState stateOf(const Reservation &reservation,
                                          const QDateTime &now);
RESERVATION_API QString displayTime(const QDateTime &value);
RESERVATION_API QString reservationStateText(
    const Reservation &reservation, const QDateTime &now,
    const QVector<Violation> &violations = {});
