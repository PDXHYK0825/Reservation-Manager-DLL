#include <DataStatistics/JsonStore.h>
#include <DataStatistics/StatisticsManager.h>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <QUuid>
#include <thread>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
QDateTime at(int day, int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 11, day), QTime(hour, minute));
}

Reservation reservation(int day = 3, int hour = 10)
{
    Reservation item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.studentAccount = QStringLiteral("student01");
    item.seatId = 1;
    item.startTime = at(day, hour);
    item.endTime = at(day, hour + 1);
    return item;
}

Violation violation(const Reservation &item, bool revoked = false)
{
    Violation result;
    result.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result.reservationId = item.id;
    result.recordedAt = item.endTime;
    result.revoked = revoked;
    return result;
}

AppData sample()
{
    AppData data;
    Reservation checked = reservation();
    checked.checkedInAt = checked.startTime;
    Reservation cancelled = reservation(3, 12);
    cancelled.studentAccount = QStringLiteral("student02");
    cancelled.seatId = 2;
    cancelled.flag = ReservationFlag::Cancelled;
    cancelled.closedAt = at(3, 9);
    Reservation terminated = reservation(3, 14);
    terminated.flag = ReservationFlag::Terminated;
    terminated.closedAt = at(3, 14, 30);
    Reservation otherDay = reservation(4, 8);
    data.reservations = {checked, cancelled, terminated, otherDay};
    Violation active = violation(terminated);
    active.recordedAt = at(4, 10); // 统计归属预约日期，不是登记日期。
    data.violations = {active, violation(otherDay, true)};
    return data;
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

void compareData(const AppData &actual, const AppData &expected)
{
    QCOMPARE(actual.reservations.size(), expected.reservations.size());
    QCOMPARE(actual.violations.size(), expected.violations.size());
    for (qsizetype i = 0; i < expected.reservations.size(); ++i) {
        const auto &a = actual.reservations[i];
        const auto &e = expected.reservations[i];
        QCOMPARE(a.id, e.id);
        QCOMPARE(a.studentAccount, e.studentAccount);
        QCOMPARE(a.seatId, e.seatId);
        QCOMPARE(a.startTime, e.startTime);
        QCOMPARE(a.endTime, e.endTime);
        QCOMPARE(a.flag, e.flag);
        QCOMPARE(a.checkedInAt, e.checkedInAt);
        QCOMPARE(a.closedAt, e.closedAt);
    }
    for (qsizetype i = 0; i < expected.violations.size(); ++i) {
        const auto &a = actual.violations[i];
        const auto &e = expected.violations[i];
        QCOMPARE(a.id, e.id);
        QCOMPARE(a.reservationId, e.reservationId);
        QCOMPARE(a.recordedAt, e.recordedAt);
        QCOMPARE(a.revoked, e.revoked);
    }
}
}

class DataModuleTests : public QObject
{
    Q_OBJECT
private slots:
    void missingFileInitializesEmpty()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        QVERIFY(store.load().ok);
        QVERIFY(store.data().reservations.isEmpty());
        QVERIFY(store.data().violations.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    void roundTripAndSignals()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore writer(path);
        QSignalSpy saved(&writer, &JsonStore::dataChanged);
        const AppData data = sample();
        const auto result = writer.commit(data);
        QVERIFY2(result.ok, qPrintable(result.message));
        QCOMPARE(saved.count(), 1);
        compareData(writer.data(), data);
        JsonStore reader(path);
        QSignalSpy loaded(&reader, &JsonStore::dataChanged);
        QVERIFY(reader.load().ok);
        QCOMPARE(loaded.count(), 1);
        compareData(reader.data(), data);
        QVERIFY(writer.commit({}).ok);
        QVERIFY(reader.load().ok);
        QVERIFY(reader.data().reservations.isEmpty());
        QVERIFY(reader.data().violations.isEmpty());
    }

    void customPathsAreIsolated()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonStore first(directory.filePath(QStringLiteral("first.json")));
        JsonStore second(directory.filePath(QStringLiteral("second.json")));
        QVERIFY(first.commit(sample()).ok);
        QVERIFY(second.load().ok);
        QVERIFY(second.data().reservations.isEmpty());
    }

    void invalidDestination_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("empty-path") << 0;
        QTest::newRow("parent-not-created") << 1;
        QTest::newRow("destination-is-directory") << 2;
    }

    void invalidDestination()
    {
        QFETCH(int, kind);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = kind == 0 ? QString()
            : kind == 1 ? directory.filePath(QStringLiteral("missing/data.json"))
                        : directory.path();
        JsonStore store(path);
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = store.commit(sample());
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        QVERIFY(store.data().reservations.isEmpty());
        QCOMPARE(changed.count(), 0);
        if (kind != 1)
            QVERIFY(!store.load().ok);
    }

    void failedSaveCanBeRetried()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        QVERIFY(QDir().mkdir(path));
        JsonStore store(path);
        QVERIFY(!store.commit(sample()).ok);
        QVERIFY(store.data().reservations.isEmpty());
        QVERIFY(QDir().rmdir(path));
        QVERIFY(store.commit(sample()).ok);
        QCOMPARE(store.data().reservations.size(), 4);
    }

    void lockedSavePreservesDiskAndMemory()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        const AppData initial = sample();
        QVERIFY(store.commit(initial).ok);
        const QByteArray before = readBytes(path);
        struct FileLock {
            HANDLE handle;
            ~FileLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } lock{CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        QVERIFY(lock.handle != INVALID_HANDLE_VALUE);
        AppData updated = initial;
        updated.reservations.push_back(reservation(5, 8));
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = store.commit(updated);
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        compareData(store.data(), initial);
        QCOMPARE(changed.count(), 0);
        CloseHandle(lock.handle);
        lock.handle = INVALID_HANDLE_VALUE;
        QCOMPARE(readBytes(path), before);
        QVERIFY(store.commit(updated).ok);
#else
        QSKIP("Windows file-sharing failure scenario");
#endif
    }

    void shortWindowsLockCanRecover()
    {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        QVERIFY(store.commit(sample()).ok);
        const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
            GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(handle != INVALID_HANDLE_VALUE);
        std::thread unlocker([handle] {
            QThread::msleep(15);
            CloseHandle(handle);
        });
        AppData updated = store.data();
        updated.reservations[0].studentAccount = QStringLiteral("changed");
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = store.commit(updated);
        unlocker.join();
        QVERIFY2(result.ok, qPrintable(result.message));
        QCOMPARE(store.data().reservations[0].studentAccount, QStringLiteral("changed"));
        QCOMPARE(changed.count(), 1);
#else
        QSKIP("Windows transient file lock scenario");
#endif
    }

    void consecutiveAtomicSaves()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        AppData data = sample();
        for (int i = 0; i < 60; ++i) {
            data.reservations[0].studentAccount = QStringLiteral("student%1").arg(i);
            const auto result = store.commit(data);
            QVERIFY2(result.ok, qPrintable(result.message));
        }
        JsonStore restored(path);
        QVERIFY(restored.load().ok);
        compareData(restored.data(), data);
    }

    void rejectsInvalidCommit_data()
    {
        QTest::addColumn<int>("kind");
        const char *names[] = {"bad-reservation-id", "duplicate-reservation-id",
            "empty-account", "seat-zero", "seat-over-legacy-limit", "bad-time",
            "end-before-start", "unaligned-start", "checkin-too-early",
            "checkin-at-end", "active-with-close-time", "closed-without-time",
            "close-at-end", "termination-before-start", "bad-flag",
            "bad-violation-id", "duplicate-violation-id", "orphan-violation",
            "bad-violation-time", "duplicate-active-violation"};
        for (int i = 0; i < 20; ++i)
            QTest::newRow(names[i]) << i;
    }

    void rejectsInvalidCommit()
    {
        QFETCH(int, kind);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        const AppData initial = sample();
        QVERIFY(store.commit(initial).ok);
        const QByteArray before = readBytes(path);
        AppData bad = initial;
        auto &r = bad.reservations[0];
        switch (kind) {
        case 0: r.id = QStringLiteral("invalid"); break;
        case 1: bad.reservations.push_back(r); break;
        case 2: r.studentAccount = QStringLiteral(" "); break;
        case 3: r.seatId = 0; break;
        case 4: r.seatId = 21; break;
        case 5: r.startTime = {}; break;
        case 6: r.endTime = r.startTime.addSecs(-1800); break;
        case 7: r.startTime = r.startTime.addSecs(60); break;
        case 8: r.checkedInAt = r.startTime.addSecs(-60); break;
        case 9: r.checkedInAt = r.endTime; break;
        case 10: r.closedAt = r.startTime; break;
        case 11: r.flag = ReservationFlag::Cancelled; break;
        case 12: r.flag = ReservationFlag::Cancelled; r.closedAt = r.endTime; break;
        case 13: r.flag = ReservationFlag::Terminated;
            r.closedAt = r.startTime.addSecs(-60); break;
        case 14: r.flag = static_cast<ReservationFlag>(99); break;
        case 15: bad.violations[0].id = QStringLiteral("bad"); break;
        case 16: bad.violations.push_back(bad.violations[0]); break;
        case 17: bad.violations[0].reservationId =
            QUuid::createUuid().toString(QUuid::WithoutBraces); break;
        case 18: bad.violations[0].recordedAt = {}; break;
        case 19: { Violation duplicate = bad.violations[0];
            duplicate.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            bad.violations.push_back(duplicate); break; }
        }
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = store.commit(bad);
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        compareData(store.data(), initial);
        QCOMPARE(readBytes(path), before);
        QCOMPARE(changed.count(), 0);
    }

    void rejectsCorruptLoad_data()
    {
        QTest::addColumn<int>("kind");
        const char *names[] = {"broken-json", "array-root", "missing-array",
            "extra-root-field", "wrong-array-type", "wrong-record-type",
            "fractional-seat", "unknown-flag", "bad-datetime",
            "missing-required-field", "non-bool-revoked", "duplicate-record",
            "orphan-violation", "wrong-string-type"};
        for (int i = 0; i < 14; ++i)
            QTest::newRow(names[i]) << i;
    }

    void rejectsCorruptLoad()
    {
        QFETCH(int, kind);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("data.json"));
        JsonStore store(path);
        const AppData initial = sample();
        QVERIFY(store.commit(initial).ok);
        QJsonObject root = QJsonDocument::fromJson(readBytes(path)).object();
        QJsonArray reservations = root[QStringLiteral("reservations")].toArray();
        QJsonArray violations = root[QStringLiteral("violations")].toArray();
        QJsonObject r = reservations[0].toObject();
        QJsonObject v = violations[0].toObject();
        switch (kind) {
        case 6: r[QStringLiteral("seatId")] = 1.5; break;
        case 7: r[QStringLiteral("flag")] = QStringLiteral("unknown"); break;
        case 8: r[QStringLiteral("startTime")] = QStringLiteral("2026-02-30T10:00"); break;
        case 9: r.remove(QStringLiteral("checkedInAt")); break;
        case 10: v[QStringLiteral("revoked")] = 0; break;
        case 12: v[QStringLiteral("reservationId")] =
            QUuid::createUuid().toString(QUuid::WithoutBraces); break;
        case 13: r[QStringLiteral("studentAccount")] = 42; break;
        }
        reservations[0] = r;
        violations[0] = v;
        if (kind == 5) reservations[0] = 42;
        if (kind == 11) reservations.append(r);
        root[QStringLiteral("reservations")] = reservations;
        root[QStringLiteral("violations")] = violations;
        if (kind == 2) root.remove(QStringLiteral("violations"));
        if (kind == 3) root[QStringLiteral("extra")] = 1;
        if (kind == 4) root[QStringLiteral("violations")] = 42;
        QByteArray bytes = QJsonDocument(root).toJson();
        if (kind == 0) bytes = "{broken";
        if (kind == 1) bytes = "[]";
        QVERIFY(writeBytes(path, bytes));
        QSignalSpy changed(&store, &JsonStore::dataChanged);
        const auto result = store.load();
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        compareData(store.data(), initial);
        QCOMPARE(readBytes(path), bytes);
        QCOMPARE(changed.count(), 0);
    }

    void legacySeatsRemainReadable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("legacy.json")));
        AppData data;
        auto r = reservation();
        r.seatId = 20;
        data.reservations.push_back(r);
        QVERIFY(store.commit(data).ok);
        QVERIFY(store.load().ok);
        QCOMPARE(store.data().reservations[0].seatId, 20);
    }

    void statisticsDateBoundaries_data()
    {
        QTest::addColumn<int>("first");
        QTest::addColumn<int>("last");
        QTest::addColumn<int>("total");
        QTest::addColumn<int>("cancelled");
        QTest::addColumn<int>("checked");
        QTest::addColumn<int>("violations");
        QTest::newRow("same-day-inclusive") << 3 << 3 << 3 << 1 << 1 << 1;
        QTest::newRow("next-day-revoked-excluded") << 4 << 4 << 1 << 0 << 0 << 0;
        QTest::newRow("both-days") << 3 << 4 << 4 << 1 << 1 << 1;
        QTest::newRow("empty-period") << 5 << 6 << 0 << 0 << 0 << 0;
    }

    void statisticsDateBoundaries()
    {
        QFETCH(int, first); QFETCH(int, last); QFETCH(int, total);
        QFETCH(int, cancelled); QFETCH(int, checked); QFETCH(int, violations);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("stats.json")));
        QVERIFY(store.commit(sample()).ok);
        StatisticsManager manager(store);
        User admin;
        admin.account = QStringLiteral("admin");
        admin.role = UserRole::Admin;
        StatisticsSummary summary;
        QVERIFY(manager.summarize(admin, QDate(2026, 11, first),
                                 QDate(2026, 11, last), summary).ok);
        QCOMPARE(summary.reservationCount, total);
        QCOMPARE(summary.cancelledCount, cancelled);
        QCOMPARE(summary.checkedInCount, checked);
        QCOMPARE(summary.activeViolationCount, violations);
    }

    void deniedStatisticsClearPreviousResult_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("student") << 0;
        QTest::newRow("blank-admin") << 1;
        QTest::newRow("reversed-range") << 2;
        QTest::newRow("invalid-start") << 3;
        QTest::newRow("invalid-end") << 4;
    }

    void deniedStatisticsClearPreviousResult()
    {
        QFETCH(int, kind);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("stats.json")));
        QVERIFY(store.commit(sample()).ok);
        StatisticsManager manager(store);
        User user;
        user.account = kind == 1 ? QStringLiteral(" ") : QStringLiteral("admin");
        user.role = kind == 0 ? UserRole::Student : UserRole::Admin;
        QDate from(2026, 11, 3), to(2026, 11, 4);
        if (kind == 2) std::swap(from, to);
        if (kind == 3) from = {};
        if (kind == 4) to = {};
        StatisticsSummary summary{99, 99, 99, 99};
        const auto result = manager.summarize(user, from, to, summary);
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        QCOMPARE(summary.reservationCount, 0);
        QCOMPARE(summary.cancelledCount, 0);
        QCOMPARE(summary.checkedInCount, 0);
        QCOMPARE(summary.activeViolationCount, 0);
    }
};

QTEST_GUILESS_MAIN(DataModuleTests)
#include "DataModuleTests.moc"
