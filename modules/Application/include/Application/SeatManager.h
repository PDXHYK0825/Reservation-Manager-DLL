#pragma once

// 成员 A：固定座位、当前状态及所选时段可预约情况。
#include "Models.h"

class JsonStore;

class SeatManager
{
public:
    explicit SeatManager(const JsonStore &store);
    QVector<Seat> listSeats() const;
    QVector<SeatStateView> getSeatStates(
        const QDateTime &start, const QDateTime &end,
        const QDateTime &now) const;

private:
    const JsonStore &dataStore;
};
