// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINMODULEINTERFACE_V2_H
#define LOGINMODULEINTERFACE_V2_H

#include "base_module_interface.h"
#include "login_module_interface.h"

namespace dss {
namespace module_v2 {

const QString LOGIN_API_VERSION = "2.0.0";

using AuthResult = dss::module::AuthResult;
using AppType = dss::module::AppType;
using AuthType = dss::module::AuthType;
using AuthState = dss::module::AuthState;
using DefaultAuthLevel = dss::module::DefaultAuthLevel;
using AppDataPtr = dss::module::AppDataPtr;
using MessageCallbackFunc = dss::module::MessageCallbackFunc;

/**
 * @brief 认证插件需要传回的数据
 */
struct AuthCallbackData {
    int result = AuthResult::None;  // 认证结果
    QString account;                // 账户
    QString token;                  // 令牌
    QString message;                // 提示消息
    QString json;                   // 预留数据
};

/**
 * @brief 验证回调函数
 * @param const AuthCallbackData * 需要传回的验证数据
 * @param void * 登录器回传指针
 * void *: ContainerPtr
 */
using AuthCallbackFun = void (*)(const AuthCallbackData *, void *);

/**
 * @brief 验证对象类型
 *
 */
enum AuthObjectType {
    LightDM,            // light display manager 显示管理器
    DeepinAuthenticate  // deepin authenticate service 深度认证框架
};

class LoginModuleInterfaceV2 : public dss::module::BaseModuleInterface
{
public:
    /**
     * @brief 模块的类型
     * @return ModuleType 见`ModuleType`说明
     */
    ModuleType type() const override { return LoginType; }

    /**
     * @brief 插件图标
     * @return 图标的绝对路径或者图标的名称(能够通过QIcon::fromTheme获取)
     */
    virtual QString icon() const { return ""; }

    /**
     * @brief 设置回调验证回调函数，此函数会在init函数之前调用
     *
     * @param AuthCallbackFunc 详见`AuthCallbackFunc`说明
     *
     * @since 2.0.0
     */
    virtual void setAuthCallback(AuthCallbackFun) = 0;

    /**
     * @brief 插件需要在这个方法中重置UI和验证状态
     * 调用时机：通常验证开始之前登录器会调用这个方法，但是不保证每次验证前都会调用。
     *
     * @since 2.0.0
     */
    virtual void reset() = 0;

    /**
     * @brief 设置消息回调函数回调函数 CallbackFun
     */
    virtual void setMessageCallback(MessageCallbackFunc) {}

    /**
     * @brief 设置登录器的回调指针，插件需要保存指针，在使用回调函数的时候回传给登录器
     *
     * 如果要使用回调函数，则必须实现此函数。
     * 函数可能会被重复调用，插件只需要保证回传的时候是最后一次设置的即可。
     *
     * @since 2.0.0
     */
    virtual void setAppData(AppDataPtr) {}

    /**
     * @brief 登录器通过此函数发送消息给插件，获取插件的信息或者同步登录器等。
     *
     * 使用json格式字符串传输数据，具体协议见技术文档。
     *
     * @since 2.0.0
     */
    virtual QString message(const QString &) { return "{}"; }

    // Warning: 不要增加虚函数，即使在最后面（如果派生类也有虚函数，那么虚表寻址也会错误）
};

} // namespace module_v2
} // namespace dss

Q_DECLARE_INTERFACE(dss::module_v2::LoginModuleInterfaceV2, "com.deepin.dde.shell.Modules_v2.Login")

#endif // LOGINMODULEINTERFACE_V2_H
