// 成员 B：学生预约、我的预约和管理员预约查询页面。
#include "Reservation/ReservationPage.h"
#include "Reservation/ReservationManager.h"
#include "Reservation/ReservationRules.h"
#include "DataStatistics/JsonStore.h"

ReservationPage::ReservationPage(
    ReservationManager &manager, const JsonStore &store, const User &user,
    const QVector<User> &knownUsers, QWidget *parent, bool historyOnly)
    : QWidget(parent), manager(manager), store(store), user(user),
      knownUsers(knownUsers)
{
    auto *root = new QVBoxLayout(this);
    controlsPanel = new QWidget;
    auto *panelLayout = new QVBoxLayout(controlsPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new QHBoxLayout;
    if (user.role == UserRole::Student && !historyOnly) {
        create = new QPushButton(QStringLiteral("预约所选座位"));
        create->setMinimumSize(200, 56);
        QFont font = create->font();
        font.setPointSize(14);
        create->setFont(font);
        controls->addWidget(create);
        connect(create, &QPushButton::clicked, this, [this] {
            message->setText(this->manager.createReservation(
                this->user, seatId, start, end,
                QDateTime::currentDateTime()).message);
        });
        setSelection(-1, {}, {});
    } else if (user.role == UserRole::Student && historyOnly) {
        auto *cancel = new QPushButton(QStringLiteral("取消所选预约"));
        controls->addWidget(cancel);
        connect(cancel, &QPushButton::clicked, this, [this] {
            const int row = table->currentRow();
            if (row < 0) {
                message->setText(QStringLiteral("请先选择一条预约。"));
                return;
            }
            message->setText(this->manager.cancelReservation(
                this->user,
                table->item(row, 0)->data(Qt::UserRole).toString(),
                QDateTime::currentDateTime()).message);
        });
    } else if (user.role == UserRole::Admin) {
        from = new QDateEdit(QDate::currentDate());
        to = new QDateEdit(QDate::currentDate());
        for (QDateEdit *edit : {from, to}) {
            edit->setCalendarPopup(true);
            edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        }
        account = new QComboBox;
        account->addItem(QStringLiteral("全部学生"), QString());
        for (const User &candidate : knownUsers) {
            if (candidate.role == UserRole::Student)
                account->addItem(candidate.account, candidate.account);
        }
        controls->addWidget(new QLabel(QStringLiteral("预约日期")));
        controls->addWidget(from);
        controls->addWidget(new QLabel(QStringLiteral("至")));
        controls->addWidget(to);
        controls->addWidget(account);
        auto *query = new QPushButton(QStringLiteral("查询"));
        controls->addWidget(query);
        connect(query, &QPushButton::clicked,
                this, &ReservationPage::refresh);
    }
    controls->addStretch();
    panelLayout->addLayout(controls);
    message = new QLabel;
    panelLayout->addWidget(message);
    root->addWidget(controlsPanel);
    if (user.role == UserRole::Student && !historyOnly)
        return;

    table = new QTableWidget(0, 6);
    table->setHorizontalHeaderLabels({
        QStringLiteral("学生账号"), QStringLiteral("座位"),
        QStringLiteral("开始时间"), QStringLiteral("结束时间"),
        QStringLiteral("状态"), QStringLiteral("签到时间")
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(table);
}

void ReservationPage::setSelection(int id, const QDateTime &begin,
                                   const QDateTime &finish)
{
    seatId = id;
    start = begin;
    end = finish;
    if (create) {
        create->setEnabled(
            id > 0 && begin >= toMinute(QDateTime::currentDateTime()));
    }
}

void ReservationPage::refresh()
{
    if (!table)
        return;
    QString selected;
    if (table->currentRow() >= 0)
        selected = table->item(table->currentRow(), 0)
            ->data(Qt::UserRole).toString();
    table->setSortingEnabled(false);
    table->setRowCount(0);
    if (from && from->date() > to->date()) {
        message->setText(QStringLiteral("起始日期不能晚于结束日期。"));
        table->setSortingEnabled(true);
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    for (const Reservation &r : manager.listReservations(user)) {
        if (from && (r.startTime.date() < from->date()
                     || r.startTime.date() > to->date()
                     || (!account->currentData().toString().isEmpty()
                         && r.studentAccount
                             != account->currentData().toString()))) {
            continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        const QStringList values = {
            r.studentAccount,
            QStringLiteral("%1号").arg(r.seatId, 2, 10, QLatin1Char('0')),
            displayTime(r.startTime), displayTime(r.endTime),
            reservationStateText(r, now, store.data().violations),
            displayTime(r.checkedInAt)
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setData(Qt::UserRole, r.id);
            table->setItem(row, column, item);
        }
    }
    table->setSortingEnabled(true);
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->data(Qt::UserRole).toString()
            == selected) {
            table->selectRow(row);
            break;
        }
    }
}
