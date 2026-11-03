#pragma once

// 成员 A：主窗口、角色页面组装、刷新和退出登录。
#include "Models.h"
#include "Application/SeatManager.h"
#include "Reservation/ReservationManager.h"
#include "AttendanceViolation/AttendanceViolationManager.h"
#include "DataStatistics/StatisticsManager.h"

#include <QMainWindow>

class JsonStore;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const User &user, JsonStore &store);

    bool loggedOut = false;

signals:
    void sessionClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool refreshing = false;
    SeatManager seats;
    ReservationManager reservations;
    AttendanceViolationManager attendance;
    StatisticsManager statistics;
};
