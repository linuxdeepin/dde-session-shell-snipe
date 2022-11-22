// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHCUTOM_H
#define AUTHCUTOM_H

#include "auth_module.h"
#include "authcommon.h"
#include "userinfo.h"
#include "login_plugin.h"

#include <QVBoxLayout>

class SessionBaseModel;

class AuthCustom : public AuthModule
{
    Q_OBJECT

    enum AuthObjectType {
        LightDM,                // light display manager - 显示服务器
        DeepinAuthenticate      // deepin authentication framework - 深度认证框架
    };


public:

    explicit AuthCustom(QWidget *parent = nullptr);
    ~AuthCustom();

    void setModule(LoginPlugin *module);
    LoginPlugin* getModule() const;
    void setAuthState(const int state, const QString &result) override;
    LoginPlugin::AuthCallbackData getCurrentAuthData() const { return m_currentAuthData; }
    void setModel(const SessionBaseModel *model);
    const SessionBaseModel *getModel() const { return m_model; }
    void initUi();
    void resetAuth();
    void reset();
    QSize contentSize() const;

    LoginPlugin::AuthType authType() const;
    LoginPlugin::PluginConfig pluginConfig() const;
    void sendAuthToken();
    void lightdmAuthStarted();

    void notifyAuthState(AuthCommon::AuthType authType, AuthCommon::AuthState state);
    using AuthModule::setLimitsInfo; // 避免警告：XXX hides overloaded virtual function
    void setLimitsInfo(const QMap<int, User::LimitsInfo> &limitsInfo);

protected:
    bool event(QEvent *e) override;

private:
    void setCallback();
    static void authCallback(const LoginPlugin::AuthCallbackData *callbackData, void *app_data);
    static QString messageCallback(const QString &message, void *app_data);
    void setAuthData(const LoginPlugin::AuthCallbackData &callbackData);
    void updateConfig();
    void changeAuthType(AuthCommon::AuthType type);
    static AuthCustom *getAuthCustomObj(void *app_data);
    static QJsonObject getDataObj(const QString &jsonStr);
    static QJsonObject getRootObj(const QString &jsonStr);

Q_SIGNALS:
    void requestCheckAccount(const QString &account);
    void requestSendToken(const QString &token);
    void notifyResizeEvent();
    void notifyAuthTypeChange(const int authType);

private:
    QVBoxLayout *m_mainLayout;
    LoginPlugin *m_plugin;
    LoginPlugin::AuthCallbackData m_currentAuthData;
    const SessionBaseModel *m_model;
    static QList<AuthCustom*> AuthCustomObjs;
};

#endif // AUTHCUTOM_H
