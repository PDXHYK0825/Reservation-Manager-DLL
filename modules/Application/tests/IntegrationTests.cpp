#include <Application/DemoData.h>
#include <Application/AppFlow.h>
#include <Application/LoginDialog.h>
#include <Application/MainWindow.h>
#include <Application/SeatManager.h>
#include <Reservation/ReservationManager.h>
#include <Reservation/ReservationRules.h>
#include <AttendanceViolation/AttendanceViolationManager.h>
#include <DataStatistics/JsonStore.h>
#include <DataStatistics/StatisticsManager.h>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <QtWidgets>

namespace {
User account(const QString &name)
{
    for (const auto &user : DemoData::users()) if (user.account == name) return user;
    return {};
}
QPushButton *button(QWidget *parent, const QString &text)
{
    for (auto *candidate : parent->findChildren<QPushButton *>())
        if (candidate->text() == text) return candidate;
    return nullptr;
}
Reservation currentReservation(const QString &user = QStringLiteral("student01"))
{
    const QDateTime now = QDateTime::currentDateTime();
    Reservation record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.studentAccount = user; record.seatId = 1;
    record.startTime = QDateTime(now.date(), QTime(now.time().hour(), (now.time().minute() / 30) * 30));
    record.endTime = record.startTime.addSecs(1800);
    return record;
}
QVector<int> values(QWidget *page)
{
    QVector<int> result;
    auto *form = page->findChild<QFormLayout *>();
    if (!form) return result;
    for (int row = 0; row < 4; ++row) {
        auto *item = form->itemAt(row, QFormLayout::FieldRole);
        auto *label = item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
        if (label) result.push_back(label->text().toInt());
    }
    return result;
}
bool containsLabel(QWidget *page, const QString &text)
{
    for (auto *label : page->findChildren<QLabel *>()) if (label->text().contains(text)) return true;
    return false;
}
void snapshot(QWidget &window, const QString &fileName)
{
    const QString directory = qEnvironmentVariable("RESERVATION_SCREENSHOT_DIR");
    if (!directory.isEmpty()) {
        QApplication::processEvents();
        window.grab().save(QDir(directory).filePath(fileName));
    }
}
}

class IntegrationTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setStyle(QStringLiteral("Fusion"));
    }

    void login_data()
    {
        QTest::addColumn<QString>("name"); QTest::addColumn<int>("role");
        QTest::newRow("admin") << QStringLiteral("admin") << int(UserRole::Admin);
        QTest::newRow("student01") << QStringLiteral("student01") << int(UserRole::Student);
        QTest::newRow("student02") << QStringLiteral("student02") << int(UserRole::Student);
    }
    void login()
    {
        QFETCH(QString, name); QFETCH(int, role);
        bool interacted = false, timedOut = false;
        QTimer watchdog; watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, [&] {
            timedOut = true;
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        watchdog.start(3000);
        QTimer::singleShot(0, [&] {
            auto *dialog = QApplication::activeModalWidget();
            if (!dialog) return;
            auto *nameEdit = dialog->findChild<QLineEdit *>(QStringLiteral("loginAccount"));
            auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("loginPassword"));
            auto *submit = button(dialog, QStringLiteral("登录"));
            if (!nameEdit || !password || !submit) return;
            nameEdit->setText(name); password->setText(QStringLiteral("123456"));
            snapshot(*dialog, QStringLiteral("login-%1.png").arg(name));
            interacted = true; QTest::mouseClick(submit, Qt::LeftButton);
        });
        User user; const bool accepted = showLogin(user); watchdog.stop();
        QVERIFY(!timedOut); QVERIFY(interacted); QVERIFY(accepted);
        QCOMPARE(user.account, name); QCOMPARE(int(user.role), role);
    }

    void wrongLogin_data()
    {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("passwordText");
        QTest::newRow("wrong-password") << QStringLiteral("student01") << QStringLiteral("wrong");
        QTest::newRow("unknown-user") << QStringLiteral("unknown") << QStringLiteral("123456");
    }
    void wrongLogin()
    {
        QFETCH(QString, name); QFETCH(QString, passwordText);
        bool rejectedCredentials = false;
        QTimer watchdog; watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        }); watchdog.start(3000);
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); if (!dialog) return;
            auto *nameEdit = dialog->findChild<QLineEdit *>(QStringLiteral("loginAccount"));
            auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("loginPassword"));
            auto *submit = button(dialog, QStringLiteral("登录"));
            if (!nameEdit || !password || !submit) { dialog->reject(); return; }
            nameEdit->setText(name); password->setText(passwordText);
            QTest::mouseClick(submit, Qt::LeftButton);
            rejectedCredentials = dialog->isVisible() && password->text().isEmpty()
                && containsLabel(dialog, QStringLiteral("账号或密码错误"));
            dialog->reject();
        });
        User user; user.account = QStringLiteral("unchanged");
        QVERIFY(!showLogin(user)); watchdog.stop();
        QVERIFY(rejectedCredentials); QCOMPARE(user.account, QStringLiteral("unchanged"));
    }

    void studentBookingConflictAndCancellation()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("ui.json"));
        JsonStore store(path); QVERIFY(store.load().ok);
        MainWindow window(account(QStringLiteral("student01")), store); window.show();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("mainTabs")); QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        auto *seatPage = window.findChild<QWidget *>(QStringLiteral("seatPage")); QVERIFY(seatPage);
        auto *start = seatPage->findChild<QComboBox *>(QStringLiteral("startTime")); QVERIFY(start);
        auto *end = seatPage->findChild<QComboBox *>(QStringLiteral("endTime")); QVERIFY(end);
        auto *booking = window.findChild<QWidget *>(QStringLiteral("reservationPage")); QVERIFY(booking);
        auto *create = button(booking, QStringLiteral("预约所选座位")); QVERIFY(create);
        auto *firstSeat = seatPage->findChild<QPushButton *>(QStringLiteral("seat1")); QVERIFY(firstSeat);
        const QTime now = QTime::currentTime();
        const int minute = qMax(480, ((now.hour()*60 + now.minute()+29)/30)*30);
        if (minute >= 1380) {
            QTest::mouseClick(firstSeat, Qt::LeftButton);
            QVERIFY(!create->isEnabled()); QVERIFY(store.data().reservations.isEmpty());
            snapshot(window, QStringLiteral("student-closed.png")); return;
        }
        const int index = start->findData(QTime(minute/60, minute%60)); QVERIFY(index >= 0);
        start->setCurrentIndex(index);
        const int endIndex = end->findData(QTime((minute+30)/60, (minute+30)%60)); QVERIFY(endIndex >= 0);
        end->setCurrentIndex(endIndex);
        QTest::mouseClick(firstSeat, Qt::LeftButton); QVERIFY(create->isEnabled());
        QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(store.data().reservations.size(), 1);
        QVERIFY(firstSeat->text().contains(QStringLiteral("已有预约")));
        auto *secondSeat = seatPage->findChild<QPushButton *>(QStringLiteral("seat2")); QVERIFY(secondSeat);
        QTest::mouseClick(secondSeat, Qt::LeftButton); QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(store.data().reservations.size(), 1);
        QVERIFY(containsLabel(booking, QStringLiteral("已有其他预约")));
        snapshot(window, QStringLiteral("student-booking.png"));
        tabs->setCurrentIndex(1);
        auto *history = window.findChild<QWidget *>(QStringLiteral("reservationHistory")); QVERIFY(history);
        auto *table = history->findChild<QTableWidget *>(); QVERIFY(table); QCOMPARE(table->rowCount(), 1);
        table->selectRow(0);
        auto *cancel = button(history, QStringLiteral("取消所选预约")); QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
        QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Cancelled);
        QCOMPARE(table->item(0,4)->text(), QStringLiteral("已取消"));
        JsonStore restored(path); QVERIFY(restored.load().ok);
        QCOMPARE(restored.data().reservations[0].flag, ReservationFlag::Cancelled);
        snapshot(window, QStringLiteral("student-history.png"));
    }

    void studentCheckInUpdatesSeat()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("ui.json")));
        AppData data; data.reservations.push_back(currentReservation()); QVERIFY(store.commit(data).ok);
        MainWindow window(account(QStringLiteral("student01")), store); window.show();
        auto *page = window.findChild<QWidget *>(QStringLiteral("attendancePage")); QVERIFY(page);
        auto *checkIn = button(page, QStringLiteral("签到")); QVERIFY(checkIn);
        QTest::mouseClick(checkIn, Qt::LeftButton);
        QVERIFY(store.data().reservations[0].checkedInAt.isValid());
        auto *seat = window.findChild<QPushButton *>(QStringLiteral("seat1")); QVERIFY(seat);
        QVERIFY(seat->text().contains(QStringLiteral("使用中")));
        QTest::mouseClick(checkIn, Qt::LeftButton);
        QVERIFY(containsLabel(page, QStringLiteral("已签到")));
    }

    void adminViolationRevokeAndStatistics()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("ui.json")));
        AppData data; data.reservations.push_back(currentReservation()); QVERIFY(store.commit(data).ok);
        MainWindow window(account(QStringLiteral("admin")), store); window.show();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("mainTabs")); QVERIFY(tabs); QCOMPARE(tabs->count(), 4);
        tabs->setCurrentIndex(2);
        auto *page = window.findChild<QWidget *>(QStringLiteral("attendancePage")); QVERIFY(page);
        auto *add = button(page, QStringLiteral("登记违约")); QVERIFY(add);
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(store.data().violations.size(), 1);
        QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Terminated);
        auto *stats = window.findChild<QWidget *>(QStringLiteral("statisticsPage")); QVERIFY(stats);
        QCOMPARE(values(stats), QVector<int>({1,0,0,1}));
        auto *table = page->findChild<QTableWidget *>(); QVERIFY(table); QCOMPARE(table->rowCount(), 1);
        table->selectRow(0);
        auto *revoke = button(page, QStringLiteral("撤销所选违约")); QVERIFY(revoke);
        QTest::mouseClick(revoke, Qt::LeftButton);
        QVERIFY(store.data().violations[0].revoked);
        QCOMPARE(store.data().reservations[0].flag, ReservationFlag::Terminated);
        QCOMPARE(values(stats), QVector<int>({1,0,0,0}));
        snapshot(window, QStringLiteral("admin-violations.png"));
        tabs->setCurrentIndex(3); snapshot(window, QStringLiteral("admin-statistics.png"));
        window.close();
        MainWindow studentWindow(account(QStringLiteral("student01")), store); studentWindow.show();
        auto *studentTabs = studentWindow.findChild<QTabWidget *>(QStringLiteral("mainTabs")); QVERIFY(studentTabs);
        studentTabs->setCurrentIndex(1);
        auto *history = studentWindow.findChild<QWidget *>(QStringLiteral("reservationHistory")); QVERIFY(history);
        auto *historyTable = history->findChild<QTableWidget *>(); QVERIFY(historyTable);
        QVERIFY(historyTable->item(0,4)->text().contains(QStringLiteral("已撤销")));
        auto *details = studentWindow.findChild<QTableWidget *>(QStringLiteral("violationHistoryTable")); QVERIFY(details);
        QCOMPARE(details->rowCount(),1);
        QCOMPARE(details->item(0,0)->text(),QStringLiteral("student01"));
        QCOMPARE(details->item(0,4)->text(),QStringLiteral("已撤销"));
        snapshot(studentWindow,QStringLiteral("student-violation-details.png"));
        studentWindow.close();
        MainWindow otherStudent(account(QStringLiteral("student02")),store);
        auto *otherDetails = otherStudent.findChild<QTableWidget *>(QStringLiteral("violationHistoryTable")); QVERIFY(otherDetails);
        QCOMPARE(otherDetails->rowCount(),0);
    }

    void automaticExpiryIsIdempotent()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("ui.json"));
        JsonStore store(path); AppData data;
        Reservation expired = currentReservation();
        expired.endTime = expired.startTime; expired.startTime = expired.endTime.addSecs(-1800);
        data.reservations.push_back(expired); QVERIFY(store.commit(data).ok);
        MainWindow window(account(QStringLiteral("admin")), store); window.show();
        QCOMPARE(store.data().violations.size(), 1);
        auto *refresh = button(&window, QStringLiteral("刷新")); QVERIFY(refresh);
        QTest::mouseClick(refresh, Qt::LeftButton); QCOMPARE(store.data().violations.size(), 1);
        AttendanceViolationManager manager(store);
        QVERIFY(manager.revokeViolation(account(QStringLiteral("admin")), store.data().violations[0].id).ok);
        QTest::mouseClick(refresh, Qt::LeftButton);
        QCOMPARE(store.data().violations.size(), 1); QVERIFY(store.data().violations[0].revoked);
        JsonStore restored(path); QVERIFY(restored.load().ok);
        MainWindow reopened(account(QStringLiteral("admin")), restored);
        QCOMPARE(restored.data().violations.size(), 1); QVERIFY(restored.data().violations[0].revoked);
    }

    void statisticsDateSelection()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("ui.json"))); AppData data;
        auto today = currentReservation(); today.checkedInAt = today.startTime;
        auto yesterday = today; yesterday.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        yesterday.startTime = yesterday.startTime.addDays(-1); yesterday.endTime = yesterday.endTime.addDays(-1);
        yesterday.checkedInAt = yesterday.startTime; data.reservations = {today,yesterday};
        QVERIFY(store.commit(data).ok);
        MainWindow window(account(QStringLiteral("admin")), store); window.show();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("mainTabs")); QVERIFY(tabs); tabs->setCurrentIndex(3);
        auto *page = window.findChild<QWidget *>(QStringLiteral("statisticsPage")); QVERIFY(page);
        QCOMPARE(values(page), QVector<int>({1,0,1,0}));
        const auto edits = page->findChildren<QDateEdit *>(); QCOMPARE(edits.size(), 2);
        edits[0]->setDate(QDate::currentDate().addDays(-1));
        auto *query = button(page, QStringLiteral("查询统计")); QVERIFY(query); QTest::mouseClick(query, Qt::LeftButton);
        QCOMPARE(values(page), QVector<int>({2,0,2,0}));
        edits[0]->setDate(QDate::currentDate().addDays(1)); QTest::mouseClick(query, Qt::LeftButton);
        QCOMPARE(values(page), QVector<int>({0,0,0,0})); QVERIFY(containsLabel(page, QStringLiteral("无效")));
    }

    void logoutClosesWindow()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("ui.json")));
        MainWindow window(account(QStringLiteral("student01")), store); window.show();
        auto *logout = button(&window, QStringLiteral("退出登录")); QVERIFY(logout);
        QTest::mouseClick(logout, Qt::LeftButton); QVERIFY(window.loggedOut); QVERIFY(!window.isVisible());
    }

    void fullCrossDllLifecycle()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("integration.json"));
        JsonStore store(path); QVERIFY(store.load().ok);
        ReservationManager reservations(store); AttendanceViolationManager attendance(store);
        StatisticsManager statistics(store); SeatManager seats(store);
        const QDate date(2026,11,3); const QDateTime now(date,QTime(9,0));
        const QDateTime start(date,QTime(10,0)), end(date,QTime(11,0));
        QVERIFY(reservations.createReservation(account(QStringLiteral("student01")),1,start,end,now).ok);
        QVERIFY(reservations.createReservation(account(QStringLiteral("student02")),1,end,end.addSecs(3600),now).ok);
        const QString first = store.data().reservations[0].id, second = store.data().reservations[1].id;
        QVERIFY(reservations.cancelReservation(account(QStringLiteral("student02")),second,now).ok);
        QVERIFY(attendance.checkIn(account(QStringLiteral("student01")),first,start).ok);
        QVERIFY(attendance.addViolation(account(QStringLiteral("admin")),first,start.addSecs(1800)).ok);
        QVERIFY(attendance.revokeViolation(account(QStringLiteral("admin")),store.data().violations[0].id).ok);
        StatisticsSummary summary;
        QVERIFY(statistics.summarize(account(QStringLiteral("admin")),date,date,summary).ok);
        QCOMPARE(summary.reservationCount,2); QCOMPARE(summary.cancelledCount,1);
        QCOMPARE(summary.checkedInCount,1); QCOMPARE(summary.activeViolationCount,0);
        const auto states = seats.getSeatStates(start,end,start.addSecs(1800)); QCOMPARE(states.size(),12);
        QCOMPARE(states[0].currentState,SeatCurrentState::Free); QVERIFY(states[0].availableForSelectedPeriod);
        JsonStore restored(path); QVERIFY(restored.load().ok);
        QCOMPARE(restored.data().reservations.size(),2);
        QCOMPARE(restored.data().reservations[0].flag,ReservationFlag::Terminated);
        QCOMPARE(restored.data().reservations[1].flag,ReservationFlag::Cancelled);
        QVERIFY(restored.data().violations[0].revoked);
    }

    void loginLogoutAndRoleSwitch()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        JsonStore store(directory.filePath(QStringLiteral("sessions.json")));
        int phase = 0; bool wrongRole = false, timedOut = false;
        QTimer steps, watchdog; steps.setInterval(30); watchdog.setSingleShot(true);
        connect(&steps, &QTimer::timeout, [&] {
            if (phase == 0 || phase == 2) {
                auto *dialog = QApplication::activeModalWidget(); if (!dialog) return;
                auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("loginAccount"));
                auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("loginPassword"));
                auto *submit = button(dialog, QStringLiteral("登录"));
                if (!name || !password || !submit) return;
                name->setText(phase == 0 ? QStringLiteral("student01") : QStringLiteral("admin"));
                password->setText(QStringLiteral("123456")); ++phase;
                QTest::mouseClick(submit, Qt::LeftButton); return;
            }
            for (auto *top : QApplication::topLevelWidgets()) {
                auto *window = dynamic_cast<MainWindow *>(top);
                if (!window || !window->isVisible()) continue;
                auto *tabs = window->findChild<QTabWidget *>(QStringLiteral("mainTabs"));
                if (!tabs) continue;
                if (phase == 1) {
                    wrongRole = wrongRole || tabs->count() != 2;
                    auto *logout = button(window, QStringLiteral("退出登录")); if (!logout) return;
                    phase = 2; QTest::mouseClick(logout, Qt::LeftButton); return;
                }
                if (phase == 3) {
                    wrongRole = wrongRole || tabs->count() != 4;
                    phase = 4; window->close(); return;
                }
            }
        });
        connect(&watchdog, &QTimer::timeout, [&] {
            timedOut = true;
            for (auto *top : QApplication::topLevelWidgets()) top->close();
        });
        steps.start(); watchdog.start(5000);
        const int result = runUserSessions(store);
        steps.stop(); watchdog.stop();
        QCOMPARE(result, 0); QVERIFY(!timedOut); QVERIFY(!wrongRole); QCOMPARE(phase,4);
    }
};

QTEST_MAIN(IntegrationTests)
#include "IntegrationTests.moc"
