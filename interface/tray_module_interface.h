// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYMODULEINTERFACE_H
#define TRAYMODULEINTERFACE_H

#include "base_module_interface.h"

#include <QWidget>

const QString TRAY_API_VERSION = "1.1.0";

namespace dss {
namespace module {

class TrayModuleInterface : public BaseModuleInterface
{
public:
    /**
     * @brief 模块的类型
     * @return ModuleType
     */
    ModuleType type() const override { return TrayType; }

    /**
    * @brief 模块加载的类型
    * @return LoadType
    */
    LoadType loadPluginType()  const override { return Load; }

    /**
     * @brief 插件图标
     * @return 图标的绝对路径或者图标的名称(能够通过QIcon::fromTheme获取)
     */
    virtual QString icon() const = 0;

    /**
     * @brief itemTipsWidget 插件的图标界面，比icon接口的图标优先级更高
     * @param itemKey
     * @return
     */
    virtual QWidget *itemWidget() const = 0;

    /**
     * @brief itemTipsWidget 插件的tip界面
     * @param itemKey
     * @return
     */
    virtual QWidget *itemTipsWidget() const = 0;

    /**
     * @brief itemContextMenu 插件的右键菜单，其格式可参考项目的示例network插件
     * @return
     */
    virtual const QString itemContextMenu() const = 0;

    /**
     * @brief invokedMenuItem 插件菜单点击后的响应实现，由插件自行实现，锁屏会在合适的时机进行调用
     * @param menuId 菜单的id
     * @param checked 菜单是否是勾选状态
     */
    virtual void invokedMenuItem(const QString &menuId, const bool checked) const = 0;

    /**
     * @brief 设置消息回调函数回调函数 MessageCallbackFunc
     *
     * @since 2.0.0
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

} // namespace module
} // namespace dss

Q_DECLARE_INTERFACE(dss::module::TrayModuleInterface, "com.deepin.dde.shell.Modules.Tray")

#endif // TRAYMODULEINTERFACE_H
