// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include "fullscreenbackground.h"

#include <memory>

class LoginContent;
class SessionBaseModel;
class User;

class LoginWindow : public FullscreenBackground
{
    Q_OBJECT

public:
    explicit LoginWindow(SessionBaseModel *const model, QWidget *parent = nullptr);

signals:
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetKeyboardLayout(std::shared_ptr<User> user, const QString &layout);

    void requestCreateAuthController(const QString &account);
    void requestDestoryAuthController(const QString &account);
    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();
    void requestCheckAccount(const QString &account);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;

private:
    LoginContent *m_loginContent;
    SessionBaseModel *m_model;
};

#endif // LOGINWINDOW_H
