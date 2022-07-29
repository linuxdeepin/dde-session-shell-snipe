// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYMODULEINTERFACE_H
#define TRAYMODULEINTERFACE_H

#include "base_module_interface.h"

#include <QWidget>

const QString TRAY_API_VERSION = "2.0.0";

namespace dss {
namespace module {

class TrayModuleInterface : public BaseModuleInterface
{
public:
    /**
     * @brief 模块的类型
     * @return ModuleType
     *
     * @since 1.0.0
     */
    ModuleType type() const override { return TrayType; }

    /**
     * @brief itemWidget 插件的图标界面，比icon接口的图标优先级更高
     * @param itemKey
     * @return  插件图标控件指针
     *
     * @since 1.0.0
     */
    virtual QWidget *itemWidget() const = 0;

    /**
     * @brief itemTipsWidget 插件的tip界面
     * @param itemKey
     * @return
     *
     * @since 1.0.0
     */
    virtual QWidget *itemTipsWidget() const = 0;

    /**
     * @brief itemContextMenu 插件的右键菜单，其格式可参考项目的示例network插件
     * @return
     *
     * @since 1.0.0
     */
    virtual const QString itemContextMenu() const = 0;

    /**
     * @brief invokedMenuItem 插件菜单点击后的响应实现，由插件自行实现，锁屏会在合适的时机进行调用
     * @param menuId 菜单的id
     * @param checked 菜单是否是勾选状态
     *
     * @since 1.0.0
     */
    virtual void invokedMenuItem(const QString &menuId, const bool checked) const = 0;
};

} // namespace module
} // namespace dss

Q_DECLARE_INTERFACE(dss::module::TrayModuleInterface, "com.deepin.dde.shell.Modules.Tray")

#endif // TRAYMODULEINTERFACE_H
