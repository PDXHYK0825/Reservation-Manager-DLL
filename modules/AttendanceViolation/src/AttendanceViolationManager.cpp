// 成员 C：签到、违约登记、查询和撤销。
#include "AttendanceViolation/AttendanceViolationManager.h"
#include "Reservation/ReservationRules.h"
#include "DataStatistics/JsonStore.h"

#include <QUuid>

AttendanceViolationManager::AttendanceViolationManager(JsonStore &store)
    : dataStore(store)
{
}

int AttendanceViolationManager::findReservation(
    const AppData &data, const QString &reservationId) const
{
    for (int i = 0; i < data.reservations.size(); ++i) {
        if (data.reservations[i].id == reservationId)
            return i;
    }
    return -1;
}

OperationResult AttendanceViolationManager::checkIn(
    const User &user, const QDateTime &now)
{
    if (user.role != UserRole::Student || user.account.trimmed().isEmpty())
        return {false, QStringLiteral("仅学生可以签到")};

    const QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
        return {false, QStringLiteral("操作时间无效")};

    const AppData &data = dataStore.data();
    bool hasReservation = false;
    QString reservationId;
    for (const Reservation &reservation : data.reservations) {
        if (reservation.studentAccount != user.account
            || reservation.flag != ReservationFlag::Active) {
            continue;
        }

        hasReservation = true;
        if (reservation.startTime.isValid()
            && reservation.endTime.isValid()
            && currentTime >= reservation.startTime
            && currentTime < reservation.endTime) {
            reservationId = reservation.id;
            break;
        }
    }

    if (reservationId.isEmpty()) {
        return {false, hasReservation
            ? QStringLiteral("当前时间不在预约时段内")
            : QStringLiteral("你还没有有效预约，请先预约座位")};
    }
    return checkIn(user, reservationId, currentTime);
}

OperationResult AttendanceViolationManager::checkIn(
    const User &user, const QString &reservationId, const QDateTime &now)
{
    if (user.role != UserRole::Student || user.account.trimmed().isEmpty())
        return {false, QStringLiteral("仅学生可以签到")};

    const QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
        return {false, QStringLiteral("操作时间无效")};

    AppData data = dataStore.data();
    const int index = findReservation(data, reservationId);
    if (index == -1)
        return {false, QStringLiteral("预约不存在")};

    Reservation &reservation = data.reservations[index];
    if (reservation.studentAccount != user.account)
        return {false, QStringLiteral("只能对本人的预约签到")};
    if (reservation.flag != ReservationFlag::Active)
        return {false, QStringLiteral("已取消或已终止的预约不能签到")};
    if (!reservation.startTime.isValid()
        || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime) {
        return {false, QStringLiteral("预约时间无效")};
    }
    if (currentTime < reservation.startTime
        || currentTime >= reservation.endTime) {
        return {false, QStringLiteral("请在预约时段内签到")};
    }
    if (reservation.checkedInAt.isValid())
        return {false, QStringLiteral("该预约已签到")};

    reservation.checkedInAt = currentTime;
    OperationResult result = dataStore.commit(data);
    if (result.ok)
        result.message = QStringLiteral("签到成功");
    return result;
}

OperationResult AttendanceViolationManager::addViolation(
    const User &admin, const QString &reservationId, const QDateTime &now)
{
    if (admin.role != UserRole::Admin || admin.account.trimmed().isEmpty())
        return {false, QStringLiteral("仅管理员可以登记违约")};

    const QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
        return {false, QStringLiteral("操作时间无效")};

    AppData data = dataStore.data();
    const int index = findReservation(data, reservationId);
    if (index == -1)
        return {false, QStringLiteral("预约不存在")};

    Reservation &reservation = data.reservations[index];
    if (!reservation.startTime.isValid()
        || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime) {
        return {false, QStringLiteral("预约时间无效")};
    }
    if (currentTime < reservation.startTime)
        return {false, QStringLiteral("预约开始后才能登记违约")};

    for (const Violation &violation : data.violations) {
        if (violation.reservationId == reservationId
            && !violation.revoked) {
            return {false, QStringLiteral("该预约已有未撤销的违约记录")};
        }
    }

    Violation violation;
    violation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    violation.reservationId = reservationId;
    violation.recordedAt = currentTime;
    violation.revoked = false;
    data.violations.append(violation);

    // 同时登记违约并终止仍有效的预约。
    if (reservation.flag == ReservationFlag::Active
        && currentTime < reservation.endTime) {
        reservation.flag = ReservationFlag::Terminated;
        reservation.closedAt = currentTime;
    }

    OperationResult result = dataStore.commit(data);
    if (result.ok)
        result.message = QStringLiteral("违约登记成功");
    return result;
}

OperationResult AttendanceViolationManager::revokeViolation(
    const User &admin, const QString &violationId)
{
    if (admin.role != UserRole::Admin || admin.account.trimmed().isEmpty())
        return {false, QStringLiteral("仅管理员可以撤销违约")};

    AppData data = dataStore.data();
    for (Violation &violation : data.violations) {
        if (violation.id != violationId)
            continue;
        if (violation.revoked)
            return {false, QStringLiteral("该违约记录已撤销")};

        // 保留记录，不恢复原预约。
        violation.revoked = true;
        OperationResult result = dataStore.commit(data);
        if (result.ok)
            result.message = QStringLiteral("违约已撤销");
        return result;
    }

    return {false, QStringLiteral("违约记录不存在")};
}

OperationResult AttendanceViolationManager::recordExpiredReservations(
    const QDateTime &now)
{
    const QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
        return {false, QStringLiteral("操作时间无效")};

    AppData data = dataStore.data();
    bool changed = false;
    for (const Reservation &reservation : data.reservations) {
        if (reservation.flag != ReservationFlag::Active
            || !reservation.startTime.isValid()
            || !reservation.endTime.isValid()
            || reservation.startTime >= reservation.endTime
            || currentTime < reservation.endTime
            || reservation.checkedInAt.isValid()) {
            continue;
        }

        bool recorded = false;
        for (const Violation &violation : data.violations) {
            if (violation.reservationId == reservation.id) {
                recorded = true;
                break;
            }
        }
        // 已撤销的违约也保留处理结果，避免下次刷新又自动登记。
        if (recorded)
            continue;

        Violation violation;
        violation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        violation.reservationId = reservation.id;
        violation.recordedAt = currentTime;
        data.violations.append(violation);
        changed = true;
    }

    if (!changed)
        return {true, QString()};
    return dataStore.commit(data);
}

QVector<Violation> AttendanceViolationManager::listViolations(
    const User &user) const
{
    QVector<Violation> records;
    if (user.account.trimmed().isEmpty())
        return records;

    const AppData &data = dataStore.data();
    if (user.role == UserRole::Admin)
        return data.violations;
    if (user.role != UserRole::Student)
        return records;

    for (const Violation &violation : data.violations) {
        for (const Reservation &reservation : data.reservations) {
            if (violation.reservationId == reservation.id
                && reservation.studentAccount == user.account) {
                records.append(violation);
                break;
            }
        }
    }
    return records;
}
