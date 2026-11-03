#pragma once

#include <QTime>

// 成员 A 维护的系统配置契约。其他成员只读取，不在各自模块内重复定义。
namespace DemoData {
inline constexpr int seatRows = 3;
inline constexpr int seatColumns = 4;
inline constexpr int seatCount = seatRows * seatColumns;
inline constexpr int reservationMinutes = 30;

inline QTime openingTime()
{
    return QTime(8, 0);
}

inline QTime closingTime()
{
    return QTime(23, 0);
}
}
