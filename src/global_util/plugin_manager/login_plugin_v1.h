// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LOGIN_PLUGIN_V1_H
#define LOGIN_PLUGIN_V1_H

#include "login_module_interface.h"
#include "login_plugin.h"

#include <QObject>

namespace LoginPlugin_V1 {

const QString API_VERSION = "1.1.0";

class LoginPluginV1 : public LoginPlugin
{
    Q_OBJECT
public:
    explicit LoginPluginV1(dss::module::LoginModuleInterface* module, QObject *parent = nullptr);

    virtual QString icon() const override;

    virtual void setMessageCallback(MessageCallbackFunc) override;

    virtual void setAppData(AppDataPtr) override;

    virtual QString message(const QString &) override;

    virtual void setAuthCallback(LoginPlugin::AuthCallbackFun) override;

    virtual void reset() override;

    static void authCallBack(const dss::module::AuthCallbackData *, void *);

    static std::string messageCallBack(const std::string &, void *);

private:
    dss::module::LoginCallBack m_loginCallBack;
    static LoginPlugin::AuthCallbackFun authCallbackFunc;
    static MessageCallbackFunc messageCallbackFunc;
};

}

#endif // LOGIN_PLUGIN_V1_H
