#pragma once
#include "Models.h"
#include "AttendanceViolation/Export.h"
#include <QtWidgets>

class AttendanceViolationManager;
class JsonStore;

// 只读明细页，学生仅能看到本人记录。
class ATTENDANCE_VIOLATION_API ViolationHistoryPage : public QWidget
{
public:
    ViolationHistoryPage(AttendanceViolationManager &manager,
                         const JsonStore &store, const User &user,
                         QWidget *parent = nullptr);
    void refresh();

private:
    AttendanceViolationManager &manager;
    const JsonStore &store;
    User user;
    QTableWidget *table;
    QLabel *message;
};
