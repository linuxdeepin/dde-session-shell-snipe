// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHCUTOM_H
#define AUTHCUTOM_H

#include "auth_module.h"
#include "authcommon.h"
#include "login_module_interface.h"

#include <QVBoxLayout>

class SessionBaseModel;

class AuthCustom : public AuthModule
{
    Q_OBJECT

    enum AuthObjectType {
        LightDM,
        DeepinAuthenticate
    };

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
    void resetAuth();
    void reset();
    QSize contentSize() const;

    inline bool showAvatar() const { return m_showAvatar; };
    inline bool showUserName() const { return m_showUserName; }
    inline bool showSwithButton() const { return m_showSwitchButton; }
    inline bool showLockButton() const { return m_showLockButton; }
    inline AuthCommon::DefaultAuthLevel defaultAuthLevel() const { return m_defaultAuthLevel; }
    inline AuthCommon::AuthType authType() const { return m_authType; }
    void sendAuthToken();
    void lightdmAuthStarted();

    void notifyAuthState(AuthCommon::AuthType authType, AuthCommon::AuthState state);
    void setLimitsInfo(const QString limitsInfoStr);

protected:
    bool event(QEvent *e) override;

private:
    void setCallback();
    static void authCallBack(const dss::module::AuthCallbackData *callbackData, void *app_data);
    static std::string messageCallback(const std::string &message, void *app_data);
    void setAuthData(const dss::module::AuthCallbackData &callbackData);
    void updateConfig();
    void changeAuthType(AuthCommon::AuthType type);
    static AuthCustom *getAuthCustomObj(void *app_data);

Q_SIGNALS:
    void requestCheckAccount(const QString &account);
    void requestSendToken(const QString &token);
    void notifyResizeEvent();
    void notifyAuthTypeChange(const int authType);

private:
    QVBoxLayout *m_mainLayout;
    dss::module::LoginModuleInterface *m_module;
    dss::module::AuthCallbackData m_currentAuthData;
    const SessionBaseModel *m_model;

    bool m_showAvatar;
    bool m_showUserName;
    bool m_showSwitchButton;
    bool m_showLockButton;
    AuthCommon::AuthType m_authType;
    AuthCommon::DefaultAuthLevel m_defaultAuthLevel;
    static QList<AuthCustom*> AuthCustomObjs;
};

#endif // AUTHCUTOM_H
