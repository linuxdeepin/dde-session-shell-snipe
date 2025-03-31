// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "login_plugin_v1.h"

#include <QDebug>

namespace LoginPlugin_V1 {

#define TO_LOGIN_PLUGIN \
    dss::module::LoginModuleInterface* loginPlugin = dynamic_cast<dss::module::LoginModuleInterface *>(m_plugin);

LoginPlugin::AuthCallbackFun LoginPluginV1::authCallbackFunc = nullptr;
PluginBase::MessageCallbackFunc LoginPluginV1::messageCallbackFunc = nullptr;

LoginPluginV1::LoginPluginV1(dss::module::LoginModuleInterface* module,  QObject *parent)
    : LoginPlugin(module, parent)
{

}

PluginBase::ModuleType LoginPluginV1::type() const
{
    TO_LOGIN_PLUGIN

    if (!loginPlugin)
        return ModuleType::LoginType;

    return m_plugin->type();
}

QString LoginPluginV1::icon() const
{
    TO_LOGIN_PLUGIN

    if (!loginPlugin)
        return "";

    return QString::fromStdString(loginPlugin->icon());
}

void LoginPluginV1::setMessageCallback(MessageCallbackFunc func)
{
    messageCallbackFunc = func;
    m_loginCallBack.messageCallbackFunc = &LoginPluginV1::messageCallBack;
}

void LoginPluginV1::setAppData(AppDataPtr func)
{
    m_loginCallBack.app_data = func;
}

QString LoginPluginV1::message(const QString & msg)
{
    TO_LOGIN_PLUGIN

    if (!loginPlugin)
        return "";

    return QString::fromStdString(loginPlugin->onMessage(msg.toStdString()));
}

void LoginPluginV1::setAuthCallback(LoginPlugin::AuthCallbackFun func)
{
    TO_LOGIN_PLUGIN

    authCallbackFunc = func;
    m_loginCallBack.authCallbackFun = &LoginPluginV1::authCallBack;
    if (!m_loginCallBack.authCallbackFun || !m_loginCallBack.app_data) {
        qCWarning(DDE_SHELL) << "Auth callback funtion or app data is null";
    }

    loginPlugin->setCallback(&m_loginCallBack);
}

void LoginPluginV1::reset()
{
    TO_LOGIN_PLUGIN

    if (!loginPlugin)
        return;

    return loginPlugin->reset();
}

void LoginPluginV1::authCallBack(const dss::module::AuthCallbackData *authData, void *appData)
{
    if (!authCallbackFunc) {
        qCWarning(DDE_SHELL) << "Authentication call back function is null";
        return;
    }

    if (!appData) {
        qCWarning(DDE_SHELL) << "App data is nullptr";
        return;
    }

    LoginPlugin::AuthCallbackData authDataV2;
    authDataV2.account = QString::fromStdString(authData->account);
    authDataV2.token = QString::fromStdString(authData->token);
    authDataV2.result = authData->result;
    authDataV2.message = QString::fromStdString(authData->message);
    authDataV2.json = QString::fromStdString(authData->json);

    authCallbackFunc(&authDataV2, appData);
}

std::string LoginPluginV1::messageCallBack(const std::string & message, void *appData)
{
    if (!messageCallbackFunc) {
        qCWarning(DDE_SHELL) << "Message callback function pointer is nullptr";
        return "";
    }

    if (!appData) {
        qCWarning(DDE_SHELL) << "Appdata is nullptr";
        return "";
    }

    const QString &ret = messageCallbackFunc(QString::fromStdString(message), appData);
    return ret.toStdString();
}

} // namespace LoginPlugin_V1
