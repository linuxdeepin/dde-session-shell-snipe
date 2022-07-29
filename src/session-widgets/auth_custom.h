// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHCUTOM_H
#define AUTHCUTOM_H

#include "auth_module.h"
#include "authcommon.h"
#include "login_module_interface.h"
#include "userinfo.h"

#include <QVBoxLayout>

class SessionBaseModel;

class AuthCustom : public AuthModule
{
    Q_OBJECT

    enum AuthObjectType {
        LightDM,                // light display manager - 显示服务器
        DeepinAuthenticate      // deepin authentication framework - 深度认证框架
    };

    // 认证插件配置
    struct PluginConfig
    {
        bool showAvatar = true;         // 是否显示头像
        bool showUserName = true;       // 是否显示用户名
        bool showSwitchButton = true;   // 是否显示类型切换按钮
        bool showLockButton = true;     // 是否显示解锁按钮
        // 默认使用此认证类型的强度，详细描述见AuthCommon::DefaultAuthLevel
        AuthCommon::DefaultAuthLevel defaultAuthLevel = AuthCommon::DefaultAuthLevel::Default;
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

    inline AuthCommon::AuthType authType() const { return m_authType; }
    inline PluginConfig pluginConfig() const { return m_pluginConfig; }
    void sendAuthToken();
    void lightdmAuthStarted();

    void notifyAuthState(AuthCommon::AuthType authType, AuthCommon::AuthState state);
    using AuthModule::setLimitsInfo; // 避免警告：XXX hides overloaded virtual function
    void setLimitsInfo(const QMap<int, User::LimitsInfo> &limitsInfo);

    static bool supportDefaultUser(dss::module::LoginModuleInterface *module);
    static bool isPluginEnabled(dss::module::LoginModuleInterface *module);

protected:
    bool event(QEvent *e) override;

private:
    void setCallback();
    static void authCallback(const dss::module::AuthCallbackData *callbackData, void *app_data);
    static QString messageCallback(const QString &message, void *app_data);
    void setAuthData(const dss::module::AuthCallbackData &callbackData);
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
    dss::module::LoginModuleInterface *m_module;
    dss::module::AuthCallbackData m_currentAuthData;
    const SessionBaseModel *m_model;
    PluginConfig m_pluginConfig;
    AuthCommon::AuthType m_authType;    // TODO 太定制话，考虑删除使用其它方法替代
    static QList<AuthCustom*> AuthCustomObjs;
};

#endif // AUTHCUTOM_H
