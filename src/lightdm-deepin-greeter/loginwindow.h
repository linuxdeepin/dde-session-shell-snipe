// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include "fullscreenbackground.h"
#include "authcommon.h"

#include <memory>
#include <DConfig>

class LoginContent;
class SessionBaseModel;
class User;

class LoginWindow : public FullScreenBackground
{
    Q_OBJECT

public:
    explicit LoginWindow(SessionBaseModel *const model, QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetKeyboardLayout(std::shared_ptr<User> user, const QString &layout);

    void requestCreateAuthController(const QString &account);
    void requestDestroyAuthController(const QString &account);
    void requestStartAuthentication(const QString &account, const AuthCommon::AuthFlags authType);
    void sendTokenToAuth(const QString &account, const AuthCommon::AuthType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();
    void requestCheckAccount(const QString &account);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;

private:
    SessionBaseModel *m_model;
    DTK_CORE_NAMESPACE::DConfig *m_dconfig;
};

#endif // LOGINWINDOW_H
