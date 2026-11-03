#pragma once

// 成员 B：学生预约、我的预约和管理员预约查询页面。
#include "Models.h"
#include "Reservation/Export.h"

#include <QtWidgets>

class JsonStore;
class ReservationManager;

class RESERVATION_API ReservationPage : public QWidget
{
public:
    ReservationPage(ReservationManager &manager, const JsonStore &store,
                    const User &user, const QVector<User> &knownUsers,
                    QWidget *parent = nullptr, bool historyOnly = false);
    void refresh();
    void setSelection(int seatId, const QDateTime &start,
                      const QDateTime &end);
    QWidget *bookingControls() const { return controlsPanel; }

private:
    ReservationManager &manager;
    const JsonStore &store;
    User user;
    QVector<User> knownUsers;
    QTableWidget *table = nullptr;
    QLabel *message = nullptr;
    QWidget *controlsPanel = nullptr;
    QPushButton *create = nullptr;
    QDateEdit *from = nullptr;
    QDateEdit *to = nullptr;
    QComboBox *account = nullptr;
    int seatId = -1;
    QDateTime start;
    QDateTime end;
};
