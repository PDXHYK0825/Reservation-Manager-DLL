// 成员 A：固定座位、当前状态及所选时段可预约情况。
#include "Application/SeatManager.h"
#include "Application/DemoData.h"
#include "Reservation/ReservationRules.h"
#include "DataStatistics/JsonStore.h"
#include "SystemConfig.h"

SeatManager::SeatManager(const JsonStore &store)
    : dataStore(store)
{
}

QVector<Seat> SeatManager::listSeats() const
{
    return DemoData::seats();
}

QVector<SeatStateView> SeatManager::getSeatStates(
    const QDateTime &start, const QDateTime &end,
    const QDateTime &now) const
{
    const QDateTime currentTime = toMinute(now);
    const QDateTime localStart = start.toLocalTime();
    const QDateTime localEnd = end.toLocalTime();
    if (!currentTime.isValid() || !isHalfHourAligned(localStart)
        || !isHalfHourAligned(localEnd) || localStart >= localEnd
        || localStart.date() != currentTime.date()
        || localEnd.date() != currentTime.date()
        || localStart.time() < DemoData::openingTime()
        || localEnd.time() > DemoData::closingTime()) {
        return {};
    }

    QVector<SeatStateView> result;
    const AppData &data = dataStore.data();
    for (const Seat &seat : listSeats()) {
        SeatStateView view;
        view.seat = seat;
        view.availableForSelectedPeriod = true;
        for (const Reservation &reservation : data.reservations) {
            if (reservation.seatId != seat.id
                || reservation.flag != ReservationFlag::Active) {
                continue;
            }

            if (overlaps(localStart, localEnd,
                         reservation.startTime, reservation.endTime)) {
                view.availableForSelectedPeriod = false;
            }

            const ReservationViewState state =
                stateOf(reservation, currentTime);
            if (state == ReservationViewState::InUse) {
                view.currentState = SeatCurrentState::InUse;
            } else if (state == ReservationViewState::WaitingCheckIn
                       && view.currentState != SeatCurrentState::InUse) {
                view.currentState = SeatCurrentState::Reserved;
            }
        }
        result.append(view);
    }
    return result;
}
