#pragma once

// 成员 D：日期筛选和四项汇总数字页面。
#include "Models.h"
#include "DataStatistics/Export.h"

#include <QtWidgets>

class StatisticsManager;

class DATA_STATISTICS_API StatisticsPage : public QWidget
{
public:
    StatisticsPage(StatisticsManager &manager, const User &user,
                   QWidget *parent = nullptr);
    void refreshStatistics();

private:
    StatisticsManager &manager;
    User user;
    QDateEdit *from = nullptr;
    QDateEdit *to = nullptr;
    QLabel *message = nullptr;
    QLabel *counts[4] = {};
};
