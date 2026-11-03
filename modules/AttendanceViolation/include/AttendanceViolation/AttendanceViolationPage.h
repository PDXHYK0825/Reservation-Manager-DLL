#pragma once

// 成员 C：学生签到、本人违约查询和管理员违约管理页面。
#include "Models.h"
#include "AttendanceViolation/Export.h"

#include <QtWidgets>

class AttendanceViolationManager;
class JsonStore;

class ATTENDANCE_VIOLATION_API AttendanceViolationPage : public QWidget
{
public:
    AttendanceViolationPage(AttendanceViolationManager &manager,
                            const JsonStore &store, const User &user,
                            QWidget *parent = nullptr);
    void refresh();
    QWidget *checkInControls() const { return controlsPanel; }

private:
    AttendanceViolationManager &manager;
    const JsonStore &store;
    User user;
    QComboBox *reservations = nullptr;
    QTableWidget *violations = nullptr;
    QLabel *message = nullptr;
    QWidget *controlsPanel = nullptr;
};
