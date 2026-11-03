#pragma once

// 成员 A：时段选择和 3x4 座位网格。
#include "Models.h"

#include <QtWidgets>

class SeatManager;

class SeatPage : public QWidget
{
    Q_OBJECT

public:
    explicit SeatPage(SeatManager &manager, QWidget *parent = nullptr);
    void refresh(const QDateTime &now);

signals:
    void selectionChanged(int seatId, const QDateTime &start,
                          const QDateTime &end);

private:
    SeatManager &manager;
    QComboBox *startCombo = nullptr;
    QComboBox *endCombo = nullptr;
    QButtonGroup *buttons = nullptr;
    QLabel *message = nullptr;
    QDateTime currentTime;

    void updateEndTimes();
    void refreshSelection();
};
