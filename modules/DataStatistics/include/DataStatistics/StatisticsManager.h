#pragma once

// 成员 D：管理员统计业务。
#include "Models.h"
#include "DataStatistics/Export.h"

class JsonStore;

class DATA_STATISTICS_API StatisticsManager
{
public:
    explicit StatisticsManager(const JsonStore &store);

    OperationResult summarize(const User &admin, const QDate &from,
                              const QDate &to,
                              StatisticsSummary &summary) const;

private:
    const JsonStore &m_store;
};
