#include <Reservation/ReservationManager.h>
#include <Reservation/ReservationRules.h>
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
User student(const QString &account = QStringLiteral("student01"))
{
    User user;
    user.account = account;
    return user;
}
User admin()
{
    User user = student(QStringLiteral("admin"));
    user.role = UserRole::Admin;
    return user;
}
QByteArray bytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class ReservationTests : public QObject
{
    Q_OBJECT
private slots:
    void validReservation_data()
    {
        QTest::addColumn<int>("startHour"); QTest::addColumn<int>("startMinute");
        QTest::addColumn<int>("endHour"); QTest::addColumn<int>("endMinute");
        QTest::addColumn<int>("seat");
        QTest::newRow("opening") << 8 << 0 << 8 << 30 << 1;
        QTest::newRow("half-hour") << 10 << 30 << 11 << 0 << 2;
        QTest::newRow("closing-last-seat") << 22 << 30 << 23 << 0 << 12;
        QTest::newRow("long-period") << 8 << 0 << 23 << 0 << 3;
    }
    void validReservation()
    {
        QFETCH(int, startHour); QFETCH(int, startMinute); QFETCH(int, endHour);
        QFETCH(int, endMinute); QFETCH(int, seat);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); QVERIFY(store.load().ok);
        ReservationManager manager(store);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = manager.createReservation(student(), seat,
            at(startHour, startMinute), at(endHour, endMinute), at(7));
        QVERIFY2(result.ok, qPrintable(result.message));
        const auto records = manager.listReservations(student());
        QCOMPARE(records.size(), 1);
        QVERIFY(!records[0].id.isEmpty());
        QCOMPARE(records[0].seatId, seat);
        QCOMPARE(records[0].startTime, at(startHour, startMinute));
        QCOMPARE(records[0].endTime, at(endHour, endMinute));
        QCOMPARE(records[0].flag, ReservationFlag::Active);
        QCOMPARE(changed.count(), 1);
        JsonStore restored(path); QVERIFY(restored.load().ok);
        QCOMPARE(restored.data().reservations[0].id, records[0].id);
    }

    void rejectsBadCreation_data()
    {
        QTest::addColumn<int>("kind");
        const char *names[] = {"admin", "blank-account", "seat-zero", "seat-thirteen",
            "invalid-now", "invalid-start", "invalid-end", "start-not-half-hour",
            "end-not-half-hour", "start-has-seconds", "end-has-milliseconds",
            "tomorrow", "cross-midnight", "past-start", "before-opening",
            "after-closing", "zero-duration", "reverse-duration"};
        for (int i = 0; i < 18; ++i) QTest::newRow(names[i]) << i;
    }
    void rejectsBadCreation()
    {
        QFETCH(int, kind);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("data.json")));
        ReservationManager manager(store);
        User user = student(); int seat = 1;
        QDateTime start = at(10), end = at(11), now = at(9);
        switch (kind) {
        case 0: user = admin(); break;
        case 1: user.account = QStringLiteral(" "); break;
        case 2: seat = 0; break;
        case 3: seat = 13; break;
        case 4: now = {}; break;
        case 5: start = {}; break;
        case 6: end = {}; break;
        case 7: start = at(10, 15); break;
        case 8: end = at(11, 15); break;
        case 9: start = start.addSecs(1); break;
        case 10: end = end.addMSecs(1); break;
        case 11: start = start.addDays(1); end = end.addDays(1); break;
        case 12: start = at(22); end = at(0).addDays(1); break;
        case 13: now = at(10, 30); break;
        case 14: now = at(6); start = at(7); end = at(8); break;
        case 15: start = at(22); end = at(23, 30); break;
        case 16: end = start; break;
        case 17: end = at(9, 30); break;
        }
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = manager.createReservation(user, seat, start, end, now);
        QVERIFY(!result.ok); QVERIFY(!result.message.isEmpty());
        QVERIFY(store.data().reservations.isEmpty());
        QCOMPARE(changed.count(), 0);
    }

    void conflictsAndAdjacency_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<bool>("allowed");
        QTest::newRow("same-seat-different-user") << 0 << false;
        QTest::newRow("same-user-different-seat") << 1 << false;
        QTest::newRow("same-user-and-seat") << 2 << false;
        QTest::newRow("adjacent-after") << 3 << true;
        QTest::newRow("adjacent-before") << 4 << true;
        QTest::newRow("cancelled-ignored") << 5 << true;
        QTest::newRow("terminated-ignored") << 6 << true;
        QTest::newRow("other-user-other-seat") << 7 << true;
    }
    void conflictsAndAdjacency()
    {
        QFETCH(int, kind); QFETCH(bool, allowed);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); ReservationManager manager(store);
        QVERIFY(manager.createReservation(student(), 1, at(10), at(11), at(9)).ok);
        if (kind == 5 || kind == 6) {
            AppData data = store.data();
            auto &item = data.reservations[0];
            item.flag = kind == 5 ? ReservationFlag::Cancelled : ReservationFlag::Terminated;
            item.closedAt = kind == 5 ? at(9) : at(10);
            QVERIFY(store.commit(data).ok);
        }
        const QByteArray before = bytes(path);
        User user = kind == 0 || kind == 7 ? student(QStringLiteral("student02")) : student();
        const int seat = kind == 1 || kind == 7 ? 2 : 1;
        QDateTime start = at(10, 30), end = at(11, 30);
        if (kind == 3) { start = at(11); end = at(11, 30); }
        if (kind == 4) { start = at(9, 30); end = at(10); }
        const auto result = manager.createReservation(user, seat, start, end, at(9));
        QCOMPARE(result.ok, allowed);
        QCOMPARE(store.data().reservations.size(), allowed ? 2 : 1);
        if (!allowed) QCOMPARE(bytes(path), before);
    }

    void cancellation_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<bool>("allowed");
        QTest::newRow("owner-before-start") << 0 << true;
        QTest::newRow("owner-during") << 1 << true;
        QTest::newRow("other-student") << 2 << false;
        QTest::newRow("admin") << 3 << false;
        QTest::newRow("blank-account") << 4 << false;
        QTest::newRow("empty-id") << 5 << false;
        QTest::newRow("unknown-id") << 6 << false;
        QTest::newRow("invalid-now") << 7 << false;
        QTest::newRow("at-end") << 8 << false;
        QTest::newRow("after-end") << 9 << false;
        QTest::newRow("already-cancelled") << 10 << false;
        QTest::newRow("already-terminated") << 11 << false;
    }
    void cancellation()
    {
        QFETCH(int, kind); QFETCH(bool, allowed);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); ReservationManager manager(store);
        QVERIFY(manager.createReservation(student(), 1, at(10), at(11), at(9)).ok);
        QString id = store.data().reservations[0].id;
        User user = student(); QDateTime now = at(9);
        if (kind == 1) now = at(10, 30);
        if (kind == 2) user = student(QStringLiteral("student02"));
        if (kind == 3) user = admin();
        if (kind == 4) user.account.clear();
        if (kind == 5) id.clear();
        if (kind == 6) id = QStringLiteral("not-found");
        if (kind == 7) now = {};
        if (kind == 8) now = at(11);
        if (kind == 9) now = at(12);
        if (kind == 10 || kind == 11) {
            AppData data = store.data();
            data.reservations[0].flag = kind == 10 ? ReservationFlag::Cancelled : ReservationFlag::Terminated;
            data.reservations[0].closedAt = at(10);
            QVERIFY(store.commit(data).ok);
        }
        const auto previousFlag = store.data().reservations[0].flag;
        const QByteArray before = bytes(path);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = manager.cancelReservation(user, id, now);
        QCOMPARE(result.ok, allowed);
        QCOMPARE(changed.count(), allowed ? 1 : 0);
        QCOMPARE(store.data().reservations[0].flag, allowed ? ReservationFlag::Cancelled : previousFlag);
        if (allowed) QCOMPARE(store.data().reservations[0].closedAt, now);
        else QCOMPARE(bytes(path), before);
    }

    void queryPermissionsAndOrder()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("data.json")));
        ReservationManager manager(store);
        QVERIFY(manager.createReservation(student(), 1, at(12), at(13), at(9)).ok);
        QVERIFY(manager.createReservation(student(QStringLiteral("student02")), 2, at(10), at(11), at(9)).ok);
        QVERIFY(manager.createReservation(student(), 3, at(10), at(11), at(9)).ok);
        const auto own = manager.listReservations(student());
        QCOMPARE(own.size(), 2);
        QCOMPARE(own[0].startTime, at(10));
        QCOMPARE(own[1].startTime, at(12));
        QCOMPARE(manager.listReservations(student(QStringLiteral("student02"))).size(), 1);
        QCOMPARE(manager.listReservations(student(QStringLiteral("unknown"))).size(), 0);
        User blankAdmin = admin(); blankAdmin.account = QStringLiteral(" ");
        QVERIFY(manager.listReservations(blankAdmin).isEmpty());
        const auto all = manager.listReservations(admin());
        QCOMPARE(all.size(), 3);
        QVERIFY(all[0].startTime <= all[1].startTime);
        QVERIFY(all[0].id < all[1].id); // 相同开始时间按编号排序。
        const auto id = own[0].id;
        const auto cancelled = manager.cancelReservation(student(), id, at(9));
        QVERIFY2(cancelled.ok, qPrintable(cancelled.message));
        QCOMPARE(manager.listReservations(student()).size(), 2); // 查询包含历史。
    }

    void failedSave_data()
    {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("create-save-failure") << false;
        QTest::newRow("cancel-save-failure") << true;
    }
    void failedSave()
    {
#ifdef Q_OS_WIN
        QFETCH(bool, cancel);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path); ReservationManager manager(store);
        QVERIFY(manager.createReservation(student(), 1, at(10), at(11), at(9)).ok);
        const auto initial = store.data().reservations[0];
        const QByteArray before = bytes(path);
        struct FileLock {
            HANDLE handle;
            ~FileLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } lock{CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
            0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        QVERIFY(lock.handle != INVALID_HANDLE_VALUE);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = cancel ? manager.cancelReservation(student(), initial.id, at(9))
            : manager.createReservation(student(), 2, at(11), at(12), at(9));
        QVERIFY(!result.ok); QVERIFY(!result.message.isEmpty());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(store.data().reservations.size(), 1);
        QCOMPARE(store.data().reservations[0].id, initial.id);
        QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Active);
        CloseHandle(lock.handle); lock.handle = INVALID_HANDLE_VALUE;
        QCOMPARE(bytes(path), before);
#else
        QSKIP("Windows file-sharing failure scenario");
#endif
    }

    void reservationStates_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<int>("expected");
        QTest::newRow("upcoming") << 0 << int(ReservationViewState::Upcoming);
        QTest::newRow("start-inclusive") << 1 << int(ReservationViewState::WaitingCheckIn);
        QTest::newRow("checked-in") << 2 << int(ReservationViewState::InUse);
        QTest::newRow("end-exclusive") << 3 << int(ReservationViewState::Finished);
        QTest::newRow("cancelled") << 4 << int(ReservationViewState::Cancelled);
        QTest::newRow("terminated") << 5 << int(ReservationViewState::Terminated);
    }
    void reservationStates()
    {
        QFETCH(int, kind); QFETCH(int, expected);
        Reservation item; item.startTime = at(10); item.endTime = at(11);
        QDateTime now = kind == 0 ? at(9) : kind == 3 ? at(11) : at(10);
        if (kind == 2) item.checkedInAt = at(10);
        if (kind == 4) item.flag = ReservationFlag::Cancelled;
        if (kind == 5) item.flag = ReservationFlag::Terminated;
        QCOMPARE(int(stateOf(item, now)), expected);
    }

    void exportedTimeAndStatusRules()
    {
        QVERIFY(!toMinute({}).isValid());
        QCOMPARE(toMinute(at(10).addSecs(59)), at(10));
        QVERIFY(isHalfHourAligned(at(10, 30)));
        QVERIFY(!isHalfHourAligned(at(10, 15)));
        QVERIFY(!isHalfHourAligned(at(10).addSecs(1)));
        QVERIFY(overlaps(at(10), at(11), at(10, 30), at(12)));
        QVERIFY(!overlaps(at(10), at(11), at(11), at(12)));
        QVERIFY(!overlaps(at(10), at(11), at(9), at(10)));
        QCOMPARE(displayTime({}), QStringLiteral("-"));
        QCOMPARE(displayTime(at(10)), QStringLiteral("2026-11-03 10:00"));
        Reservation item; item.id = QStringLiteral("id");
        item.startTime = at(10); item.endTime = at(11);
        Violation v; v.reservationId = item.id;
        QCOMPARE(reservationStateText(item, at(9), {v}), QStringLiteral("违约"));
        v.revoked = true;
        QVERIFY(reservationStateText(item, at(9), {v}).contains(QStringLiteral("已撤销")));
    }
};

QTEST_GUILESS_MAIN(ReservationTests)
#include "ReservationTests.moc"
