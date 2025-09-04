// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODULE_INTERFACE_H
#define MODULE_INTERFACE_H

#include <QtCore>

const QString BASE_API_VERSION = "2.0.0";

namespace dss {
namespace module {

/**
 * @brief 消息回调函数
 * @param const QString & 发送给登录器的数据
 * @param void * 登录器回传指针
 *
 * @return 登录器返回的数据
 * @since 2.0.0
 *
 * 传入和传出的数据都是json格式的字符串，详见登录插件接口说明文档
 *
 */
using MessageCallbackFunc = QString (*)(const QString&, void *);

using AppDataPtr = void*;

class BaseModuleInterface
{
public:
    /**
     * @brief The ModuleType enum
     * 模块的类型
     */
    enum ModuleType {
        LoginType,   // 登陆插件
        TrayType,     // 托盘插件
        FullManagedLoginType,    // 全托管插件
        IpcAssistLoginType,       // 用于接收厂商密码插件
        PasswordExtendLoginType,    // 密码认证扩展插件，需要两个都认证通过后
    };

    /**
    * @brief The LoadType enum
    * 模块加载的类型
    */
    enum LoadType {
        Load,       // 加载插件
        Notload     // 不加载插件
    };

    virtual ~BaseModuleInterface() = default;

    /**
     * @brief 界面相关的初始化
     * 插件在非主线程加载，故界面相关的初始化需要放在这个方法里，由主程序调用并初始化
     *
     * @since 1.0.0
     */
    virtual void init() = 0;

    /**
     * @brief 键值，用于与其它模块区分
     * @return QString 插件的唯一标识，由插件自己定义
     *
     * @since 1.0.0
     */
    virtual QString key() const = 0;

    /**
     * @brief 模块想显示的界面
     * content 对象由模块自己管理
     * @return QWidget*
     *
     * @since 1.0.0
     */
    virtual QWidget *content() = 0;

    /**
     * @brief 模块的类型 登陆器会根据不同
     * @return ModuleType 详见`ModuleType`说明
     *
     * @since 1.0.0
     */
    virtual ModuleType type() const = 0;

    /**
    * @brief 模块加载类型
    *
    * 登陆器在加载插件的时候会调用这个接口，如果返回Load，那么登陆会加载插件，返回其它则不会加载插件。
    * 非全平台或全架构的插件需要重新实现这个接口，根据实际情况处理。
    *
    * @return LoadType 详见`LoadType`说明
    *
    * @since 1.1.0
    */
    virtual LoadType loadPluginType() const { return Load; }

    // Warning: 不要增加虚函数，即使在最后面（会改变派生类的虚表）
};

} // namespace module
} // namespace dss

Q_DECLARE_INTERFACE(dss::module::BaseModuleInterface, "com.deepin.dde.shell.Modules")

#endif // MODULE_INTERFACE_H
