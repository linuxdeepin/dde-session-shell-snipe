// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "login_plugin_v2.h"

namespace LoginPlugin_V2 {

#define TO_LOGIN_PLUGIN_V2 \
    dss::module_v2::LoginModuleInterfaceV2* pluginV2 = dynamic_cast<dss::module_v2::LoginModuleInterfaceV2 *>(m_plugin);

LoginPluginV2::LoginPluginV2(dss::module_v2::LoginModuleInterfaceV2 * module, QObject *parent)
    : LoginPlugin(module, parent)
{

}

QString LoginPluginV2::icon() const
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return "";

    return pluginV2->icon();
}

void LoginPluginV2::setMessageCallback(MessageCallbackFunc func)
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return;

    return pluginV2->setMessageCallback(func);
}

void LoginPluginV2::setAppData(void* func)
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return;

    return pluginV2->setAppData(func);
}

QString LoginPluginV2::message(const QString & msg)
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return "";

    return pluginV2->message(msg);
}

void LoginPluginV2::setAuthCallback(AuthCallbackFun func)
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return;

    pluginV2->setAuthCallback(func);
}

void LoginPluginV2::reset()
{
    TO_LOGIN_PLUGIN_V2

    if (!pluginV2)
        return;

    return pluginV2->reset();
}

}
