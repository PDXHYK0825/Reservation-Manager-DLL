#include "AttendanceViolation/ViolationHistoryPage.h"
#include "AttendanceViolation/AttendanceViolationManager.h"
#include "Reservation/ReservationRules.h"
#include "DataStatistics/JsonStore.h"

ViolationHistoryPage::ViolationHistoryPage(AttendanceViolationManager &manager,
    const JsonStore &store, const User &user, QWidget *parent)
    : QWidget(parent), manager(manager), store(store), user(user)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    message = new QLabel;
    layout->addWidget(message);
    table = new QTableWidget(0, 5);
    table->setObjectName(QStringLiteral("violationHistoryTable"));
    table->setHorizontalHeaderLabels({QStringLiteral("学生账号"), QStringLiteral("座位"),
        QStringLiteral("预约时段"), QStringLiteral("登记时间"), QStringLiteral("状态")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table);
}

void ViolationHistoryPage::refresh()
{
    table->setRowCount(0);
    const auto records = manager.listViolations(user);
    message->setText(records.isEmpty() ? QStringLiteral("暂无违约记录") : QString());
    message->setVisible(records.isEmpty());
    for (const auto &violation : records) {
        for (const auto &reservation : store.data().reservations) {
            if (reservation.id != violation.reservationId)
                continue;
            const int row = table->rowCount();
            table->insertRow(row);
            const QStringList fields = {reservation.studentAccount,
                QStringLiteral("%1号").arg(reservation.seatId, 2, 10, QLatin1Char('0')),
                displayTime(reservation.startTime) + QStringLiteral("—")
                    + reservation.endTime.time().toString(QStringLiteral("HH:mm")),
                displayTime(violation.recordedAt),
                violation.revoked ? QStringLiteral("已撤销") : QStringLiteral("有效违约")};
            for (int column = 0; column < fields.size(); ++column) {
                auto *item = new QTableWidgetItem(fields[column]);
                item->setData(Qt::UserRole, violation.id);
                table->setItem(row, column, item);
            }
            break;
        }
    }
}
