// 成员 A：固定账号登录窗口。
#include "Application/LoginDialog.h"
#include "Application/DemoData.h"

#include <QtWidgets>

bool showLogin(User &user)
{
    QDialog dialog;
    dialog.setObjectName(QStringLiteral("loginDialog"));
    dialog.setWindowTitle(QStringLiteral("自习室预约系统 - 登录"));
    dialog.setMinimumWidth(360);
    auto *root = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *account = new QLineEdit;
    auto *password = new QLineEdit;
    account->setObjectName(QStringLiteral("loginAccount"));
    password->setObjectName(QStringLiteral("loginPassword"));
    password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("账号"), account);
    form->addRow(QStringLiteral("密码"), password);
    root->addLayout(form);
    auto *message = new QLabel;
    message->setObjectName(QStringLiteral("loginMessage"));
    root->addWidget(message);
    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("退出"));
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        for (const User &candidate : DemoData::users()) {
            if (candidate.account == account->text().trimmed()
                && candidate.password == password->text()) {
                user = candidate;
                password->clear();
                dialog.accept();
                return;
            }
        }
        message->setText(QStringLiteral("账号或密码错误"));
        password->clear();
        password->setFocus();
    });
    return dialog.exec() == QDialog::Accepted;
}
