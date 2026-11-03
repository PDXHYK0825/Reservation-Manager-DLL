// 成员 B：公共时间及预约状态规则。
#include "Reservation/ReservationRules.h"

QDateTime toMinute(const QDateTime &value)
{
    if (!value.isValid())
        return {};

    QDateTime local = value.toLocalTime();
    local.setTime(QTime(local.time().hour(), local.time().minute()));
    return local;
}

bool isHalfHourAligned(const QDateTime &value)
{
    if (!value.isValid())
        return false;

    const QTime time = value.time();
    return (time.minute() == 0 || time.minute() == 30)
        && time.second() == 0 && time.msec() == 0;
}

bool overlaps(const QDateTime &startA, const QDateTime &endA,
              const QDateTime &startB, const QDateTime &endB)
{
    return startA < endB && endA > startB;
}

ReservationViewState stateOf(const Reservation &reservation,
                             const QDateTime &now)
{
    const QDateTime minuteNow = toMinute(now);
    if (reservation.flag == ReservationFlag::Cancelled)
        return ReservationViewState::Cancelled;
    if (reservation.flag == ReservationFlag::Terminated)
        return ReservationViewState::Terminated;
    if (!reservation.endTime.isValid() || minuteNow >= reservation.endTime)
        return ReservationViewState::Finished;
    if (minuteNow < reservation.startTime)
        return ReservationViewState::Upcoming;
    if (reservation.checkedInAt.isValid())
        return ReservationViewState::InUse;
    return ReservationViewState::WaitingCheckIn;
}

QString displayTime(const QDateTime &value)
{
    return value.isValid()
        ? value.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("-");
}

QString reservationStateText(const Reservation &reservation,
                             const QDateTime &now,
                             const QVector<Violation> &violations)
{
    bool revoked = false;
    for (const Violation &violation : violations) {
        if (violation.reservationId != reservation.id)
            continue;
        if (!violation.revoked)
            return QStringLiteral("违约");
        revoked = true;
    }
    const QStringList names = {
        QStringLiteral("待开始"), QStringLiteral("待签到"),
        QStringLiteral("使用中"), QStringLiteral("已结束"),
        QStringLiteral("已取消"), QStringLiteral("已终止")
    };
    const QString state = names[int(stateOf(reservation, now))];
    return revoked ? state + QStringLiteral("（违约已撤销）") : state;
}
