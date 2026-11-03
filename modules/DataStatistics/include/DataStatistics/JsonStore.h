#pragma once

// 成员 D：JSON 持久化，是其他业务模块共同依赖的底层模块。
#include "Models.h"
#include "DataStatistics/Export.h"

#include <QObject>

class DATA_STATISTICS_API JsonStore : public QObject
{
    Q_OBJECT

public:
    explicit JsonStore(QObject *parent = nullptr);
    // 可指定数据文件；独立测试和调用方可使用各自的数据目录。
    explicit JsonStore(const QString &filePath, QObject *parent = nullptr);

    OperationResult load();
    const AppData &data() const;
    OperationResult commit(const AppData &updatedData);

signals:
    void dataChanged();

private:
    AppData m_data;
    QString m_filePath;
};
