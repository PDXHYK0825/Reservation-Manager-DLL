#include "Application/AppFlow.h"
#include "Application/LoginDialog.h"
#include "Application/MainWindow.h"
#include <QApplication>
#include <QEventLoop>

int runUserSessions(JsonStore &store)
{
    QApplication::setQuitOnLastWindowClosed(false);
    User user;
    while (showLogin(user)) {
        MainWindow window(user, store);
        QEventLoop session;
        const auto closed = QObject::connect(&window, &MainWindow::sessionClosed,
                                              &session, &QEventLoop::quit);
        window.show();
        session.exec();
        QObject::disconnect(closed);
        if (!window.loggedOut)
            break;
    }
    return 0;
}
