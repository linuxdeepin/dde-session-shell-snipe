// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tray_plugin.h"

/**
 * 插件统一封装，如果后面有不兼容的更新，则拆分多个版本，继承此类
 */

#define TO_TRAY_PLUGIN \
    dss::module::TrayModuleInterface* trayPlugin = dynamic_cast<dss::module::TrayModuleInterface *>(m_plugin);

TrayPlugin::TrayPlugin(dss::module::TrayModuleInterface *module, QObject *parent)
    : PluginBase(module, parent)
{

}

QString TrayPlugin::icon() const
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return "";

    return trayPlugin->icon();
}

void TrayPlugin::setMessageCallback(MessageCallbackFunc func)
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return;

    return trayPlugin->setMessageCallback(func);
}

void TrayPlugin::setAppData(void* func)
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return;

    return trayPlugin->setAppData(func);
}

QString TrayPlugin::message(const QString & msg)
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return "";

    return trayPlugin->message(msg);
}

QWidget *TrayPlugin::itemWidget() const
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return nullptr;

    return trayPlugin->itemWidget();
}

QWidget *TrayPlugin::itemTipsWidget() const
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return nullptr;

    return trayPlugin->itemTipsWidget();
}

const QString TrayPlugin::itemContextMenu() const
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return nullptr;

    return trayPlugin->itemContextMenu();
}

void TrayPlugin::invokedMenuItem(const QString &menuId, const bool checked) const
{
    TO_TRAY_PLUGIN

    if (!trayPlugin)
        return;

    trayPlugin->invokedMenuItem(menuId, checked);
}
