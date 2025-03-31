// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_plugin_util.h"

#include "login_plugin.h"
#include "plugin_manager.h"

int LPUtil::loginType()
{
    auto *plugin = getLevel2LoginPlugin();
    if (!plugin) {
        return LoginPlugin::CustomLoginType::CLT_Default;
    }

    return plugin->loginType();
}

int LPUtil::updateLoginType()
{
    auto plugin = getLevel2LoginPlugin();
    if (!plugin) {
        return LoginPlugin::CustomLoginType::CLT_Default;
    }
    return plugin->updateLoginType();
}

int LPUtil::sessionTimeout()
{
    auto plugin = getLevel2LoginPlugin();
    if (!plugin) {
        return DEFAULT_SESSION_TIMEOUT;
    }
    return plugin->sessionTimeout();
}

bool LPUtil::hasSecondLevel(const QString &user)
{
    auto plugin = getLevel2LoginPlugin();
    if (!plugin) {
        return false;
    }
    return plugin->hasSecondLevel(user);
}

LoginPlugin* LPUtil::getLevel2LoginPlugin()
{
    const auto &plugins = PluginManager::instance()->getLoginPlugins(2);
    return plugins.empty() ? nullptr : plugins.first();
}
