#pragma once

// 成员 B：创建、取消和查询预约。
#include "Models.h"
#include "Reservation/Export.h"

class JsonStore;

class RESERVATION_API ReservationManager
{
public:
    explicit ReservationManager(JsonStore &store);

    OperationResult createReservation(const User &user, int seatId,
                                      const QDateTime &start,
                                      const QDateTime &end,
                                      const QDateTime &now);
    OperationResult cancelReservation(const User &user,
                                      const QString &reservationId,
                                      const QDateTime &now);
    QVector<Reservation> listReservations(const User &user) const;

private:
    JsonStore &m_store;
};
