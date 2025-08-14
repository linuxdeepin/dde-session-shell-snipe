// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASSIST_LOGIN_WIDGET_H
#define ASSIST_LOGIN_WIDGET_H

#include <QWidget>
#include "login_module_interface.h"
#include "auth_module.h"
#include "login_plugin.h"

#include "auth_widget.h"
#include "userinfo.h"

class AssistLoginWidget: public AuthModule
{
    Q_OBJECT

public:
    explicit AssistLoginWidget(QWidget *parent = nullptr);
    ~AssistLoginWidget();

    void setModule(LoginPlugin *module);
    void initUI();
    QSize contentSize() const;
    void startAuth();
    void resetAuth();
    void updateConfig();
    void updateVisible();

    const LoginPlugin::PluginConfig &getPluginConfig() const;
    void setPluginConfig(const LoginPlugin::PluginConfig &PluginConfig);
    void setAuthState(AuthCommon::AuthState state, const QString &result);
    dss::module::BaseModuleInterface::ModuleType pluginType() const;
    bool readyToAuth() const;
    QString extraInfo() const { return m_extraInfo; }

Q_SIGNALS:
    void requestCheckAccount(const QString &account);
    void requestSendToken(const QString &account, const QString &token);
    void requestPluginConfigChanged(const LoginPlugin::PluginConfig &PluginConfig);
    void requestHidePlugin();
    void readyToAuthChanged(bool ready);
    void requestSendExtraInfo(const QString &info);

private:
    void setCallback();
    static void authCallback(const LoginPlugin::AuthCallbackData *callbackData, void *app_data);
    static QString messageCallback(const QString &message, void *app_data);
    void setAuthData(const LoginPlugin::AuthCallbackData &callbackData);
    static AssistLoginWidget *getAssistLoginWidgetObj(void *app_data);

    QVBoxLayout *m_mainLayout;
    LoginPlugin::AuthCallbackData m_currentAuthData;
    LoginPlugin *m_module;
    static QList<AssistLoginWidget *> AssistLoginWidgetObjs;
    LoginPlugin::PluginConfig m_pluginConfig;
    QString m_extraInfo;
};


#endif //ASSIST_LOGIN_WIDGET_H
