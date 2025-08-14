// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_PLUGIN_H
#define LOGIN_PLUGIN_H

#include "plugin_base.h"
#include "login_module_interface_v2.h"
#include "authcommon.h"

#include <QObject>
#include <QJsonObject>

#define DEFAULT_SESSION_TIMEOUT 15000

class LoginPlugin : public PluginBase
{
    Q_OBJECT
public:
    using AuthCallbackData = dss::module_v2::AuthCallbackData;
    using AuthCallbackFun = dss::module_v2::AuthCallbackFun;
    using AuthResult = dss::module_v2::AuthResult;
    using DefaultAuthLevel = dss::module::DefaultAuthLevel;
    using AuthType = dss::module::AuthType;
    using CustomLoginType = dss::module::CustomLoginType;

    // 认证插件配置
    struct PluginConfig
    {
        bool showAvatar = true;                     // 是否显示头像
        bool showUserName = true;                   // 是否显示用户名
        bool showSwitchButton = true;               // 是否显示类型切换按钮
        bool showLockButton = true;                 // 是否显示解锁按钮
        bool showBackGroundColor = true;            // 是否显示灰色背景
        bool switchUserWhenCheckAccount = true;     // 检测用户已经登录后是否切换到这个已登录的用户
        bool notUsedByLoginedUserInGreeter = false; // 在登录界面且当前用户已登录时不使用插件
        bool saveLastAuthType = true;               // 是否保存上一次认证类型
        // 默认使用此认证类型的强度
        DefaultAuthLevel defaultAuthLevel = DefaultAuthLevel::Default;
        AuthCommon::AuthType assignAuthType = AuthCommon::AuthType::AT_Custom;

        bool operator==(const PluginConfig b) const
        {
            return this->showAvatar == b.showAvatar &&
                   this->showLockButton == b.showAvatar &&
                   this->showSwitchButton == b.showSwitchButton &&
                   this->showUserName == b.showUserName &&
                   this->defaultAuthLevel == b.defaultAuthLevel &&
                   this->showBackGroundColor == b.showBackGroundColor &&
                   this->switchUserWhenCheckAccount == b.switchUserWhenCheckAccount &&
                   this->notUsedByLoginedUserInGreeter == b.notUsedByLoginedUserInGreeter &&
                   this->saveLastAuthType == b.saveLastAuthType &&
                   this->assignAuthType == b.assignAuthType;
        }

        bool operator!=(const PluginConfig b) const
        {
            return this->showAvatar != b.showAvatar ||
                   this->showLockButton != b.showAvatar ||
                   this->showSwitchButton != b.showSwitchButton ||
                   this->showUserName != b.showUserName ||
                   this->defaultAuthLevel != b.defaultAuthLevel ||
                   this->showBackGroundColor != b.showBackGroundColor ||
                   this->switchUserWhenCheckAccount != b.switchUserWhenCheckAccount ||
                   this->notUsedByLoginedUserInGreeter != b.notUsedByLoginedUserInGreeter ||
                   this->saveLastAuthType != b.saveLastAuthType ||
                   this->assignAuthType != b.assignAuthType;
        }
    };

    explicit LoginPlugin(dss::module::BaseModuleInterface *module, QObject *parent = nullptr);

    virtual PluginBase::ModuleType type() const = 0;

    virtual void setAuthCallback(AuthCallbackFun) = 0;

    virtual void reset() = 0;

    bool isPluginEnabled();

    int level(); // 插件所处层级

    int loginType(); // 从插件获取登录类型: 默认、多因、第三方

    bool hasSecondLevel(const QString &user); // 检查是否有第二层

    int updateLoginType(); // 通知插件更新登录类型，登录类型信息依赖远程配置

    int sessionTimeout(); // 插件自定义的会话超时时长，单位ms

    bool supportDefaultUser();

    void notifyCurrentUserChanged(const QString &userName, uid_t uid);

    void notifyAuthFactorsChanged(int authFactors);

    void updateConfig();

    void accountError();

    inline AuthType defaultAuthType() const { return m_authType; }

    inline PluginConfig pluginConfig() const { return m_pluginConfig; }

    void authStateChanged(AuthCommon::AuthType type, AuthCommon::AuthState state, const QString &prompt);

    bool readyToAuth();

private:
    PluginConfig m_pluginConfig;
    AuthType m_authType;
};

#endif // LOGIN_PLUGIN_H
