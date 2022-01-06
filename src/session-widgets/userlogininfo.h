/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lixin <lixin_cm@deepin.com>
 *
 * Maintainer: lixin <lixin_cm@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USERLOGININFO_H
#define USERLOGININFO_H

#include <QObject>
#include <QPointer>

#include <memory>

class User;
class UserLoginWidget;
class SessionBaseModel;
class UserFrameList;

class UserLoginInfo : public QObject
{
    Q_OBJECT
public:
    explicit UserLoginInfo(SessionBaseModel *model, QObject *parent = nullptr);
    void initConnect();
    void setUser(std::shared_ptr<User> user);
    void updateLocale();
    UserLoginWidget *getUserLoginWidget();
    UserFrameList *getUserFrameList();
    void hideKBLayout();
    void abortConfirm(bool abort = true);
    void beforeUnlockAction(bool is_finish);

signals:
    void switchToCurrentUser();
    void requestSwitchUser(std::shared_ptr<User> user);
    void hideUserFrameList();
    void requestSetLayout(std::shared_ptr<User> user, const QString &value);
    void unlockActionFinish();

    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);

    void requestCheckAccount(const QString &account);

private:
    void userLockChanged(bool disable);
    void receiveSwitchUser(std::shared_ptr<User> user);

private:
    bool m_shutdownAbort = false;
    std::shared_ptr<User> m_user;
    SessionBaseModel *m_model;
    QPointer<UserLoginWidget> m_userLoginWidget;
    UserFrameList *m_userFrameList;
    QList<QMetaObject::Connection> m_currentUserConnects;
};

#endif // USERLOGININFO_H
