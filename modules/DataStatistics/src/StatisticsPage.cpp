// 成员 D：日期筛选和四项汇总数字页面。
#include "DataStatistics/StatisticsPage.h"
#include "DataStatistics/StatisticsManager.h"

StatisticsPage::StatisticsPage(StatisticsManager &manager, const User &user,
                               QWidget *parent)
    : QWidget(parent), manager(manager), user(user)
{
    auto *root = new QVBoxLayout(this);
    auto *controls = new QHBoxLayout;
    from = new QDateEdit(QDate::currentDate());
    to = new QDateEdit(QDate::currentDate());
    for (QDateEdit *edit : {from, to}) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    }
    controls->addWidget(new QLabel(QStringLiteral("预约日期")));
    controls->addWidget(from);
    controls->addWidget(new QLabel(QStringLiteral("至")));
    controls->addWidget(to);
    auto *query = new QPushButton(QStringLiteral("查询统计"));
    controls->addWidget(query);
    controls->addStretch();
    root->addLayout(controls);
    message = new QLabel;
    root->addWidget(message);
    auto *form = new QFormLayout;
    const QStringList names = {QStringLiteral("预约总数"), QStringLiteral("取消数"),
        QStringLiteral("签到数"), QStringLiteral("有效违约数")};
    for (int i = 0; i < 4; ++i) {
        counts[i] = new QLabel;
        form->addRow(names[i], counts[i]);
    }
    root->addLayout(form);
    root->addStretch();
    connect(query, &QPushButton::clicked,
            this, &StatisticsPage::refreshStatistics);
}

void StatisticsPage::refreshStatistics()
{
    StatisticsSummary summary;
    const OperationResult result =
        manager.summarize(user, from->date(), to->date(), summary);
    message->setText(result.ok ? QString() : result.message);
    const int values[] = {summary.reservationCount, summary.cancelledCount,
                          summary.checkedInCount, summary.activeViolationCount};
    for (int i = 0; i < 4; ++i)
        counts[i]->setText(QString::number(values[i]));
}
