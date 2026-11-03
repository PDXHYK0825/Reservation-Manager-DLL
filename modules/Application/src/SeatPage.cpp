// 成员 A：时段选择和 3x4 座位网格。
#include "Application/SeatPage.h"
#include "Application/SeatManager.h"
#include "Reservation/ReservationRules.h"
#include "SystemConfig.h"

SeatPage::SeatPage(SeatManager &manager, QWidget *parent)
    : QWidget(parent), manager(manager)
{
    auto *root = new QVBoxLayout(this);
    auto *controls = new QHBoxLayout;
    startCombo = new QComboBox;
    endCombo = new QComboBox;
    startCombo->setObjectName(QStringLiteral("startTime"));
    endCombo->setObjectName(QStringLiteral("endTime"));
    for (QTime time = DemoData::openingTime();
         time < DemoData::closingTime();
         time = time.addSecs(DemoData::reservationMinutes * 60)) {
        startCombo->addItem(
            time.toString(QStringLiteral("HH:mm")), time);
    }
    controls->addWidget(new QLabel(QStringLiteral("当天时段　开始")));
    controls->addWidget(startCombo);
    controls->addWidget(new QLabel(QStringLiteral("结束")));
    controls->addWidget(endCombo);
    controls->addStretch();
    root->addLayout(controls);
    auto *grid = new QGridLayout;
    buttons = new QButtonGroup(this);
    for (const Seat &seat : manager.listSeats()) {
        auto *button = new QPushButton;
        button->setObjectName(QStringLiteral("seat%1").arg(seat.id));
        button->setCheckable(true);
        button->setMinimumHeight(96);
        button->setSizePolicy(
            QSizePolicy::Expanding, QSizePolicy::Expanding);
        buttons->addButton(button, seat.id);
        grid->addWidget(button,
                        (seat.id - 1) / DemoData::seatColumns,
                        (seat.id - 1) % DemoData::seatColumns);
    }
    root->addLayout(grid, 1);
    message = new QLabel;
    message->setWordWrap(true);
    root->addWidget(message);
    connect(startCombo, &QComboBox::currentIndexChanged,
            this, [this] {
        updateEndTimes();
        refreshSelection();
    });
    connect(endCombo, &QComboBox::currentIndexChanged,
            this, &SeatPage::refreshSelection);
    connect(buttons, &QButtonGroup::idClicked,
            this, &SeatPage::refreshSelection);
}

void SeatPage::updateEndTimes()
{
    const QTime oldEnd = endCombo->currentData().toTime();
    const QSignalBlocker blocker(endCombo);
    endCombo->clear();
    for (QTime time = startCombo->currentData().toTime().addSecs(
             DemoData::reservationMinutes * 60);
         time <= DemoData::closingTime();
         time = time.addSecs(DemoData::reservationMinutes * 60)) {
        endCombo->addItem(
            time.toString(QStringLiteral("HH:mm")), time);
    }
    const int index = endCombo->findData(oldEnd);
    if (index >= 0)
        endCombo->setCurrentIndex(index);
}

void SeatPage::refresh(const QDateTime &now)
{
    const bool newDate = currentTime.date() != now.date();
    currentTime = toMinute(now);
    if (newDate) {
        int index = 0;
        while (index + 1 < startCombo->count()
               && startCombo->itemData(index).toTime()
                   < currentTime.time()) {
            ++index;
        }
        const QSignalBlocker blocker(startCombo);
        startCombo->setCurrentIndex(index);
        updateEndTimes();
    }
    refreshSelection();
}

void SeatPage::refreshSelection()
{
    const QDateTime start(
        currentTime.date(), startCombo->currentData().toTime());
    const QDateTime end(
        currentTime.date(), endCombo->currentData().toTime());
    for (const SeatStateView &view :
         manager.getSeatStates(start, end, currentTime)) {
        const QString state =
            view.currentState == SeatCurrentState::InUse
            ? QStringLiteral("使用中")
            : view.currentState == SeatCurrentState::Reserved
                ? QStringLiteral("已预约")
                : QStringLiteral("空闲");
        buttons->button(view.seat.id)->setText(
            QStringLiteral("%1\n当前：%2\n所选时段：%3")
                .arg(view.seat.name, state,
                     !view.availableForSelectedPeriod
                         ? QStringLiteral("已有预约")
                         : start < currentTime
                             ? QStringLiteral("已过开始时间")
                             : QStringLiteral("可预约")));
    }
    const int id = buttons->checkedId();
    message->setText(
        id < 0
        ? QStringLiteral(
              "请选择时段及座位；当前状态与所选时段是否占用分别显示。")
        : QStringLiteral("已选择：%1号　%2—%3")
              .arg(id, 2, 10, QLatin1Char('0'))
              .arg(start.time().toString("HH:mm"),
                   end.time().toString("HH:mm")));
    emit selectionChanged(id, start, end);
}
