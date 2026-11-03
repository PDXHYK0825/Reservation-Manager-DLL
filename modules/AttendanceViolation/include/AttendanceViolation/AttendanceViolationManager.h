#pragma once

// 成员 C：签到、违约登记、查询和撤销。
#include "Models.h"
#include "AttendanceViolation/Export.h"

class JsonStore;

class ATTENDANCE_VIOLATION_API AttendanceViolationManager
{
public:
    explicit AttendanceViolationManager(JsonStore &store);

    OperationResult checkIn(const User &user, const QDateTime &now);
    OperationResult checkIn(const User &user,
                            const QString &reservationId,
                            const QDateTime &now);
    OperationResult addViolation(const User &admin,
                                 const QString &reservationId,
                                 const QDateTime &now);
    OperationResult revokeViolation(const User &admin,
                                    const QString &violationId);
    OperationResult recordExpiredReservations(const QDateTime &now);
    QVector<Violation> listViolations(const User &user) const;

private:
    JsonStore &dataStore;
    int findReservation(const AppData &data,
                        const QString &reservationId) const;
};
