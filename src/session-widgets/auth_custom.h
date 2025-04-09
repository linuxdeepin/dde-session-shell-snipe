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
    LoginPlugin* getLoginPlugin() const;
    QString loginPluginKey() const;
    void setAuthState(const AuthCommon::AuthState state, const QString &result) override;
    LoginPlugin::AuthCallbackData getCurrentAuthData() const { return m_currentAuthData; }
    void setModel(const SessionBaseModel *model);
    const SessionBaseModel *getModel() const { return m_model; }
    void initUi();
    void resetAuth();
    void reset();
    QSize contentSize() const;
    QSize contentSizeHint() const;

    LoginPlugin::PluginConfig pluginConfig() const;
    void sendAuthToken();
    void lightdmAuthStarted();

    void notifyAuthState(AuthCommon::AuthFlags authType, AuthCommon::AuthState state);
    using AuthModule::setLimitsInfo; // 避免警告：XXX hides overloaded virtual function
    void setLimitsInfo(const QMap<int, User::LimitsInfo> &limitsInfo);

    void setCustomAuthIndex(int index) { m_customAuthIndex = index; }
    int customAuthType() const { return static_cast<int>(AuthCommon::AT_Custom) + m_customAuthIndex; }

    void detachPlugin();

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
    void requestCheckAccount(const QString &account, bool switchUser = true);
    void requestSendToken(const QString &token);
    void notifyResizeEvent();
    void notifyAuthTypeChange(const AuthCommon::AuthType authType);

private:
    QVBoxLayout *m_mainLayout;
    LoginPlugin *m_plugin;
    LoginPlugin::AuthCallbackData m_currentAuthData;
    const SessionBaseModel *m_model;
    static QList<AuthCustom*> AuthCustomObjs;
    int m_customAuthIndex;
};

#endif // AUTHCUTOM_H
