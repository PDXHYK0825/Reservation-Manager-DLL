// 成员 C：学生签到、本人违约查询和管理员违约管理页面。
#include "AttendanceViolation/AttendanceViolationPage.h"
#include "Reservation/ReservationRules.h"
#include "AttendanceViolation/AttendanceViolationManager.h"
#include "DataStatistics/JsonStore.h"

AttendanceViolationPage::AttendanceViolationPage(
    AttendanceViolationManager &manager, const JsonStore &store,
    const User &user, QWidget *parent)
    : QWidget(parent), manager(manager), store(store), user(user)
{
    auto *root = new QVBoxLayout(this);
    controlsPanel = new QWidget;
    auto *panelLayout = new QVBoxLayout(controlsPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new QHBoxLayout;
    const bool admin = user.role == UserRole::Admin;
    if (admin) {
        controls->addWidget(new QLabel(QStringLiteral("选择预约")));
        reservations = new QComboBox;
        reservations->setMinimumContentsLength(45);
        controls->addWidget(reservations, 1);
    }
    auto *action = new QPushButton(
        admin ? QStringLiteral("登记违约") : QStringLiteral("签到"));
    controls->addWidget(action);
    if (!admin) {
        action->setMinimumSize(200, 56);
        QFont font = action->font();
        font.setPointSize(14);
        action->setFont(font);
        controls->addStretch();
    }
    panelLayout->addLayout(controls);
    message = new QLabel;
    message->setWordWrap(true);
    panelLayout->addWidget(message);
    root->addWidget(controlsPanel);
    connect(action, &QPushButton::clicked, this, [this, admin] {
        const QDateTime now = QDateTime::currentDateTime();
        OperationResult result;
        if (admin) {
            const QString id = reservations->currentData().toString();
            if (id.isEmpty()) {
                message->setText(QStringLiteral("请先选择一条预约。"));
                return;
            }
            result = this->manager.addViolation(this->user, id, now);
        } else {
            result = this->manager.checkIn(this->user, now);
        }
        message->setText(result.message);
    });
    if (!admin)
        return;

    auto *revoke = new QPushButton(QStringLiteral("撤销所选违约"));
    root->addWidget(revoke, 0, Qt::AlignLeft);
    connect(revoke, &QPushButton::clicked, this, [this] {
        const int row = violations->currentRow();
        if (row < 0) {
            message->setText(QStringLiteral("请先选择一条违约记录。"));
            return;
        }
        message->setText(this->manager.revokeViolation(
            this->user,
            violations->item(row, 0)
                ->data(Qt::UserRole).toString()).message);
    });

    violations = new QTableWidget(0, 5);
    violations->setHorizontalHeaderLabels({
        QStringLiteral("学生账号"), QStringLiteral("座位"),
        QStringLiteral("预约开始时间"), QStringLiteral("登记时间"),
        QStringLiteral("状态")
    });
    violations->setEditTriggers(QAbstractItemView::NoEditTriggers);
    violations->setSelectionBehavior(QAbstractItemView::SelectRows);
    violations->setSelectionMode(QAbstractItemView::SingleSelection);
    violations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(violations);
}

void AttendanceViolationPage::refresh()
{
    if (!reservations || !violations)
        return;

    const QString reservationId = reservations->currentData().toString();
    reservations->clear();
    const QDateTime now = QDateTime::currentDateTime();
    for (const Reservation &r : store.data().reservations) {
        if (user.role != UserRole::Admin
            && r.studentAccount != user.account) {
            continue;
        }
        reservations->addItem(
            QStringLiteral("%1　%2号　%3—%4　%5")
                .arg(r.studentAccount)
                .arg(r.seatId, 2, 10, QLatin1Char('0'))
                .arg(displayTime(r.startTime),
                     r.endTime.time().toString(QStringLiteral("HH:mm")),
                     reservationStateText(
                         r, now, store.data().violations)),
            r.id);
    }
    const int index = reservations->findData(reservationId);
    if (index >= 0)
        reservations->setCurrentIndex(index);

    QString selected;
    if (violations->currentRow() >= 0) {
        selected = violations->item(violations->currentRow(), 0)
            ->data(Qt::UserRole).toString();
    }
    violations->setSortingEnabled(false);
    violations->setRowCount(0);
    for (const Violation &v : manager.listViolations(user)) {
        for (const Reservation &r : store.data().reservations) {
            if (r.id != v.reservationId)
                continue;

            const int row = violations->rowCount();
            violations->insertRow(row);
            const QStringList values = {
                r.studentAccount,
                QStringLiteral("%1号")
                    .arg(r.seatId, 2, 10, QLatin1Char('0')),
                displayTime(r.startTime), displayTime(v.recordedAt),
                v.revoked ? QStringLiteral("已撤销")
                          : QStringLiteral("有效违约")
            };
            for (int column = 0; column < values.size(); ++column) {
                auto *item = new QTableWidgetItem(values[column]);
                item->setData(Qt::UserRole, v.id);
                violations->setItem(row, column, item);
            }
            break;
        }
    }
    violations->setSortingEnabled(true);
    for (int row = 0; row < violations->rowCount(); ++row) {
        if (violations->item(row, 0)->data(Qt::UserRole).toString()
            == selected) {
            violations->selectRow(row);
            break;
        }
    }
}
