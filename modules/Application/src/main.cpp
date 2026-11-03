// 成员 A：程序入口。这里只创建主窗口，以及由成员 D 导出的 JsonStore。
#include "Application/AppFlow.h"
#include "DataStatistics/JsonStore.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(QStringLiteral(
        "QPushButton, QToolButton {"
        " border: 1px solid #9a9a9a; border-radius: 0px;"
        " padding: 6px 12px; background-color: #f5f5f5; color: #202020; }"
        "QPushButton:hover, QToolButton:hover { background-color: #e5edf5; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #c5d7e8; }"
        "QPushButton:checked, QToolButton:checked {"
        " background-color: #c5d7e8; border-color: #3c6d99; }"
        "QPushButton:focus, QToolButton:focus { border: 2px solid #3c6d99; }"
        "QPushButton:disabled, QToolButton:disabled {"
        " background-color: #eeeeee; color: #888888;"
        " border-color: #cccccc; }"));
    JsonStore store;
    const OperationResult loaded = store.load();
    if (!loaded.ok) {
        QMessageBox::critical(
            nullptr, QStringLiteral("数据加载失败"), loaded.message);
        return 1;
    }
    return runUserSessions(store);
}
