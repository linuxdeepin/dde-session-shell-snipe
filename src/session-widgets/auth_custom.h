/*
 * Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
 *
 * Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
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

#ifndef AUTHCUTOM_H
#define AUTHCUTOM_H

#include "auth_module.h"
#include "login_module_interface.h"

#include <QVBoxLayout>

class SessionBaseModel;

class AuthCustom : public AuthModule
{
    Q_OBJECT

public:

    explicit AuthCustom(QWidget *parent = nullptr);
    ~AuthCustom();

    void setModule(dss::module::LoginModuleInterface *module);
    dss::module::LoginModuleInterface* getModule() const;
    void setAuthState(const int state, const QString &result) override;
    dss::module::AuthCallbackData getCurrentAuthData() const { return m_currentAuthData; }
    void setModel(const SessionBaseModel *model);
    const SessionBaseModel *getModel() const { return m_model; }
    void initUi();
    void reset();
    QSize contentSize() const;

    inline bool showAvatar() const { return m_showAvatar; };
    inline bool showUserName() const { return m_showUserName; }
    inline bool showSwithButton() const { return m_showSwitchButton; }
    inline bool showLockButton() const { return m_showLockButton; }
    inline AuthCommon::DefaultAuthLevel defaultAuthLevel() const { return m_defaultAuthLevel; }
    inline AuthCommon::AuthType authType() const { return m_authType; }
    void sendAuthToken();

protected:
    bool event(QEvent *e);

private:
    void init();
    static void authCallBack(const dss::module::AuthCallbackData *callbackData, void *app_data);
    static std::string messageCallback(const std::string &message, void *app_data);
    void setAuthData(const dss::module::AuthCallbackData &callbackData);
    void updateConfig();
    void changeAuthType(AuthCommon::AuthType type);

Q_SIGNALS:
    void requestCheckAccount(const QString &account);
    void requestSendToken(const QString &token);
    void notifyResizeEvent();
    void notifyAuthTypeChange(const int authType);

private:
    QVBoxLayout *m_mainLayout;
    dss::module::LoginCallBack m_callback;
    dss::module::LoginModuleInterface *m_module;
    dss::module::AuthCallbackData m_currentAuthData;
    const SessionBaseModel *m_model;

    bool m_showAvatar;
    bool m_showUserName;
    bool m_showSwitchButton;
    bool m_showLockButton;
    AuthCommon::AuthType m_authType;
    AuthCommon::DefaultAuthLevel m_defaultAuthLevel;
};

#endif // AUTHCUTOM_H
