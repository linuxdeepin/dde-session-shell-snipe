// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINMODULEINTERFACE_H
#define LOGINMODULEINTERFACE_H

#include "base_module_interface.h"

const QString LOGIN_API_VERSION = "1.2.0";

namespace dss {
namespace module {

enum AuthResult{
    None = 0,
    Success,
    Failure
};

/**
 * @brief 认证插件需要传回的数据
 */
struct AuthCallbackData{
    int result = AuthResult::None;          // 认证结果
    std::string account; // 账户
    std::string token;   // 令牌
    std::string message; // 提示消息
    std::string json;    // 预留数据
};

/**
 * @brief The AppType enum
 * 发起认证的应用类型
 */
enum AppType {
    Login = 1, // 登录
    Lock = 2   // 锁屏
};

/**
 * @brief
 */
enum DefaultAuthLevel {
    NoDefault = 0,  // 如果获取到上次认证成功的类型，则默认使用上次登陆成功的类型，否则根据系统默认的顺序来选择
    Default,        // 如果获取到上次认证成功的类型，则默认使用上次登陆成功的类型，否则使用插件认证
    StrongDefault   // 无论是否可以获取到上次认证成功的类型，都默认选择插件验证
};

/**
 * @brief The AuthType enum
 * 认证类型
 */
enum AuthType {
    AT_None = 0,                 // none
    AT_Password = 1 << 0,        // 密码
    AT_Fingerprint = 1 << 1,     // 指纹
    AT_Face = 1 << 2,            // 人脸
    AT_ActiveDirectory = 1 << 3, // AD域
    AT_Ukey = 1 << 4,            // ukey
    AT_FingerVein = 1 << 5,      // 指静脉
    AT_Iris = 1 << 6,            // 虹膜
    AT_PIN = 1 << 7,             // PIN
    AT_PAM = 1 << 29,            // PAM
    AT_Custom = 1 << 30,         // 自定义
    AT_All = -1                  // all
};

/**
 * @brief The AuthStatus enum
 * 认证状态
 */
enum AuthState {
    AS_None = -1,   // none
    AS_Success,     // 成功，此次认证的最终结果
    AS_Failure,     // 失败，此次认证的最终结果
    AS_Cancel,      // 取消，当认证没有给出最终结果时，调用 End 会出发 Cancel 信号
    AS_Timeout,     // 超时
    AS_Error,       // 错误
    AS_Verify,      // 验证中
    AS_Exception,   // 设备异常，当前认证会被 End
    AS_Prompt,      // 设备提示
    AS_Started,     // 认证已启动，调用 Start 之后，每种成功开启都会发送此信号
    AS_Ended,       // 认证已结束，调用 End 之后，每种成功关闭的都会发送此信号，当某种认证类型被锁定时，也会触发此信号
    AS_Locked,      // 认证已锁定，当认证类型锁定时，触发此信号。该信号不会给出锁定等待时间信息
    AS_Recover,     // 设备恢复，需要调用 Start 重新开启认证，对应 AS_Exception
    AS_Unlocked     // 认证解锁，对应 AS_Locked
};

/**
 * @brief 验证回调函数
 * @param const AuthCallbackData * 需要传回的验证数据
 * @param void * 登录器回传指针
 * void *: ContainerPtr
 */
using AuthCallbackFun = void (*)(const AuthCallbackData *, void *);

/**
 * @brief 验证回调函数
 * @param const std::string & 发送给登录器的数据
 * @param void * 登录器回传指针
 *
 * @return 登录器返回的数据
 * @since 1.2.0
 *
 * 传入和传出的数据都是json格式的字符串，详见登录插件接口说明文档
 *
 */
using MessageCallbackFun = std::string (*)(const std::string &, void *);

/**
 * @brief 插件回调相关
 */
struct LoginCallBack {
    void *app_data; // 插件无需关注
    AuthCallbackFun authCallbackFun;
    MessageCallbackFun messageCallbackFunc;
};

class LoginModuleInterface : public BaseModuleInterface
{
public:
    /**
     * @brief 模块的类型
     * @return ModuleType
     */
    ModuleType type() const override { return LoginType; }

    /**
    * @brief 模块加载的类型
    * @return LoadType
    */
    LoadType loadPluginType()  const override { return Load; }

    /**
     * @brief 模块图标的路径
     * @return std::string
     */
    virtual std::string icon() const { return ""; }

    /**
     * @brief 设置回调函数 CallbackFun
     */
    virtual void setCallback(LoginCallBack *) = 0;

    /**
     * @brief 登录器通过此函数发送消息给插件，获取插件的状态或者信息，使用json格式字符串in&out数据，方便后期扩展和定制。
     *
     * @since 1.2.0
     */
    virtual std::string onMessage(const std::string &) { return "{}"; };

    /**
     * @brief 重置插件
     */
    virtual void reset() = 0;
};

} // namespace module
} // namespace dss

Q_DECLARE_INTERFACE(dss::module::LoginModuleInterface, "com.deepin.dde.shell.Modules.Login")

#endif // LOGINMODULEINTERFACE_H
