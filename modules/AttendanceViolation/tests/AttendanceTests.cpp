#include <AttendanceViolation/AttendanceViolationManager.h>
#include <Reservation/ReservationManager.h>
#include <DataStatistics/JsonStore.h>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
QDateTime at(int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 11, 3), QTime(hour, minute));
}
User student(QString account = QStringLiteral("student01"))
{
    User user; user.account = account; return user;
}
User admin()
{
    User user = student(QStringLiteral("admin")); user.role = UserRole::Admin; return user;
}
bool seed(JsonStore &store)
{
    ReservationManager reservations(store);
    return reservations.createReservation(student(), 1, at(10), at(11), at(9)).ok;
}
QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class AttendanceTests : public QObject
{
    Q_OBJECT
private slots:
    void checkIn_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<bool>("allowed");
        QTest::newRow("start-inclusive") << 0 << true;
        QTest::newRow("last-minute") << 1 << true;
        QTest::newRow("before-start") << 2 << false;
        QTest::newRow("end-exclusive") << 3 << false;
        QTest::newRow("admin-denied") << 4 << false;
        QTest::newRow("blank-account") << 5 << false;
        QTest::newRow("other-student") << 6 << false;
        QTest::newRow("missing-record") << 7 << false;
        QTest::newRow("invalid-time") << 8 << false;
        QTest::newRow("cancelled") << 9 << false;
        QTest::newRow("terminated") << 10 << false;
        QTest::newRow("repeat-checkin") << 11 << false;
        QTest::newRow("automatic-selection") << 12 << true;
        QTest::newRow("automatic-no-current-record") << 13 << false;
        QTest::newRow("automatic-blank-account") << 14 << false;
    }
    void checkIn()
    {
        QFETCH(int, kind); QFETCH(bool, allowed);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); QVERIFY(seed(store));
        AttendanceViolationManager manager(store);
        const QString originalId = store.data().reservations[0].id;
        QString id = originalId; User user = student(); QDateTime now = at(10);
        if (kind == 1) now = at(10, 59);
        if (kind == 2 || kind == 13) now = at(9, 59);
        if (kind == 3) now = at(11);
        if (kind == 4) user = admin();
        if (kind == 5 || kind == 14) user.account = QStringLiteral("  ");
        if (kind == 6) user = student(QStringLiteral("student02"));
        if (kind == 7) id = QStringLiteral("missing");
        if (kind == 8) now = {};
        if (kind == 9 || kind == 10) {
            AppData changed = store.data();
            changed.reservations[0].flag = kind == 9 ? ReservationFlag::Cancelled : ReservationFlag::Terminated;
            changed.reservations[0].closedAt = at(10);
            QVERIFY(store.commit(changed).ok);
        }
        if (kind == 11) QVERIFY(manager.checkIn(user, id, at(10)).ok);
        const QByteArray before = readBytes(path);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = kind >= 12 ? manager.checkIn(user, now) : manager.checkIn(user, id, now);
        QCOMPARE(result.ok, allowed);
        QCOMPARE(changed.count(), allowed ? 1 : 0);
        if (allowed) {
            QCOMPARE(store.data().reservations[0].checkedInAt, now);
            JsonStore restored(path); QVERIFY(restored.load().ok);
            QCOMPARE(restored.data().reservations[0].checkedInAt, now);
        } else {
            QVERIFY(!result.message.isEmpty()); QCOMPARE(readBytes(path), before);
        }
    }

    void violation_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<bool>("allowed");
        QTest::newRow("terminate-current") << 0 << true;
        QTest::newRow("record-after-end") << 1 << true;
        QTest::newRow("student-denied") << 2 << false;
        QTest::newRow("blank-admin") << 3 << false;
        QTest::newRow("before-start") << 4 << false;
        QTest::newRow("invalid-time") << 5 << false;
        QTest::newRow("missing-record") << 6 << false;
        QTest::newRow("duplicate-active") << 7 << false;
        QTest::newRow("manual-after-revoke") << 8 << true;
    }
    void violation()
    {
        QFETCH(int, kind); QFETCH(bool, allowed);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); QVERIFY(seed(store)); AttendanceViolationManager manager(store);
        QString id = store.data().reservations[0].id;
        User user = admin(); QDateTime now = at(10, 30);
        if (kind == 1) now = at(11);
        if (kind == 2) user = student();
        if (kind == 3) user.account = QStringLiteral(" ");
        if (kind == 4) now = at(9, 59);
        if (kind == 5) now = {};
        if (kind == 6) id = QStringLiteral("missing");
        if (kind == 7 || kind == 8) {
            QVERIFY(manager.addViolation(admin(), id, at(10)).ok);
            if (kind == 8) QVERIFY(manager.revokeViolation(admin(), store.data().violations[0].id).ok);
        }
        const qsizetype count = store.data().violations.size();
        const QByteArray before = readBytes(path);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = manager.addViolation(user, id, now);
        QCOMPARE(result.ok, allowed);
        QCOMPARE(store.data().violations.size(), count + (allowed ? 1 : 0));
        QCOMPARE(changed.count(), allowed ? 1 : 0);
        if (allowed) {
            const auto &record = store.data().violations.last();
            QCOMPARE(record.reservationId, id); QCOMPARE(record.recordedAt, now); QVERIFY(!record.revoked);
            if (kind == 0) {
                QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Terminated);
                QCOMPARE(store.data().reservations[0].closedAt, now);
            }
            if (kind == 1) QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Active);
        } else { QVERIFY(!result.message.isEmpty()); QCOMPARE(readBytes(path), before); }
    }

    void revoke_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<bool>("allowed");
        QTest::newRow("admin") << 0 << true;
        QTest::newRow("student") << 1 << false;
        QTest::newRow("blank-admin") << 2 << false;
        QTest::newRow("missing-record") << 3 << false;
        QTest::newRow("already-revoked") << 4 << false;
    }
    void revoke()
    {
        QFETCH(int, kind); QFETCH(bool, allowed);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); QVERIFY(seed(store)); AttendanceViolationManager manager(store);
        QVERIFY(manager.addViolation(admin(), store.data().reservations[0].id, at(10)).ok);
        QString id = store.data().violations[0].id; User user = admin();
        if (kind == 1) user = student();
        if (kind == 2) user.account = QStringLiteral(" ");
        if (kind == 3) id = QStringLiteral("missing");
        if (kind == 4) QVERIFY(manager.revokeViolation(admin(), id).ok);
        const QByteArray before = readBytes(path);
        const auto result = manager.revokeViolation(user, id);
        QCOMPARE(result.ok, allowed);
        QCOMPARE(store.data().violations.size(), 1);
        QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Terminated);
        if (allowed) QVERIFY(store.data().violations[0].revoked);
        else { QVERIFY(!result.message.isEmpty()); QCOMPARE(readBytes(path), before); }
    }

    void automaticExpiry_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<int>("expected");
        QTest::newRow("before-end") << 0 << 0;
        QTest::newRow("at-end") << 1 << 1;
        QTest::newRow("checked-in-excluded") << 2 << 0;
        QTest::newRow("cancelled-excluded") << 3 << 0;
        QTest::newRow("terminated-excluded") << 4 << 0;
        QTest::newRow("existing-active-not-duplicated") << 5 << 1;
        QTest::newRow("revoked-not-recreated") << 6 << 1;
        QTest::newRow("invalid-time") << 7 << 0;
    }
    void automaticExpiry()
    {
        QFETCH(int, kind); QFETCH(int, expected);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("data.json"))); QVERIFY(seed(store));
        AttendanceViolationManager manager(store);
        const QString id = store.data().reservations[0].id;
        if (kind == 2) QVERIFY(manager.checkIn(student(), id, at(10)).ok);
        if (kind == 3 || kind == 4) {
            AppData data = store.data();
            data.reservations[0].flag = kind == 3 ? ReservationFlag::Cancelled : ReservationFlag::Terminated;
            data.reservations[0].closedAt = at(10); QVERIFY(store.commit(data).ok);
        }
        if (kind == 5 || kind == 6) {
            QVERIFY(manager.addViolation(admin(), id, at(11)).ok);
            if (kind == 6) QVERIFY(manager.revokeViolation(admin(), store.data().violations[0].id).ok);
        }
        const QDateTime now = kind == 0 ? at(10, 59) : kind == 7 ? QDateTime() : at(11);
        const auto result = manager.recordExpiredReservations(now);
        QCOMPARE(result.ok, kind != 7);
        QCOMPARE(store.data().violations.size(), expected);
        if (kind != 7) {
            QSignalSpy changed(&store, &JsonStore::dataChanged);
            QVERIFY(manager.recordExpiredReservations(now).ok);
            QCOMPARE(store.data().violations.size(), expected); QCOMPARE(changed.count(), 0);
        }
    }

    void queryPermissions()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("data.json"))); QVERIFY(seed(store));
        ReservationManager reservations(store); AttendanceViolationManager manager(store);
        QVERIFY(reservations.createReservation(student(QStringLiteral("student02")), 2, at(10), at(11), at(9)).ok);
        QVERIFY(manager.addViolation(admin(), store.data().reservations[0].id, at(10)).ok);
        QVERIFY(manager.addViolation(admin(), store.data().reservations[1].id, at(10)).ok);
        QVERIFY(manager.revokeViolation(admin(), store.data().violations[0].id).ok);
        QCOMPARE(manager.listViolations(admin()).size(), 2);
        QCOMPARE(manager.listViolations(student()).size(), 1);
        QVERIFY(manager.listViolations(student())[0].revoked);
        QCOMPARE(manager.listViolations(student(QStringLiteral("student02"))).size(), 1);
        QCOMPARE(manager.listViolations(student(QStringLiteral("unknown"))).size(), 0);
        User blank = admin(); blank.account = QStringLiteral(" ");
        QVERIFY(manager.listViolations(blank).isEmpty());
    }

    void saveFailure_data()
    {
        QTest::addColumn<int>("operation");
        QTest::newRow("checkin") << 0; QTest::newRow("add-violation") << 1;
        QTest::newRow("revoke") << 2; QTest::newRow("automatic-expiry") << 3;
    }
    void saveFailure()
    {
#ifdef Q_OS_WIN
        QFETCH(int, operation);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); QVERIFY(seed(store)); AttendanceViolationManager manager(store);
        const QString id = store.data().reservations[0].id;
        if (operation == 2) QVERIFY(manager.addViolation(admin(), id, at(10)).ok);
        const AppData initial = store.data(); const QByteArray before = readBytes(path);
        struct FileLock {
            HANDLE handle;
            ~FileLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } lock{CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        QVERIFY(lock.handle != INVALID_HANDLE_VALUE);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        OperationResult result;
        if (operation == 0) result = manager.checkIn(student(), id, at(10));
        if (operation == 1) result = manager.addViolation(admin(), id, at(10));
        if (operation == 2) result = manager.revokeViolation(admin(), initial.violations[0].id);
        if (operation == 3) result = manager.recordExpiredReservations(at(11));
        QVERIFY(!result.ok); QVERIFY(!result.message.isEmpty()); QCOMPARE(changed.count(), 0);
        QCOMPARE(store.data().reservations[0].flag, initial.reservations[0].flag);
        QCOMPARE(store.data().reservations[0].checkedInAt, initial.reservations[0].checkedInAt);
        QCOMPARE(store.data().reservations[0].closedAt, initial.reservations[0].closedAt);
        QCOMPARE(store.data().violations.size(), initial.violations.size());
        if (operation == 2) QVERIFY(!store.data().violations[0].revoked);
        CloseHandle(lock.handle); lock.handle = INVALID_HANDLE_VALUE;
        QCOMPARE(readBytes(path), before);
#else
        QSKIP("Windows file lock scenario");
#endif
    }
};

QTEST_GUILESS_MAIN(AttendanceTests)
#include "AttendanceTests.moc"
