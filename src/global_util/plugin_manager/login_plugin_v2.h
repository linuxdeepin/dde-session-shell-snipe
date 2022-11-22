// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LOGIN_PLUGIN_V2_H
#define LOGIN_PLUGIN_V2_H

#include "login_plugin.h"
#include "login_module_interface_v2.h"

#include <QObject>

namespace LoginPlugin_V2 {

const QString API_VERSION = "2.0.0";

class LoginPluginV2 : public LoginPlugin
{
    Q_OBJECT
public:
    explicit LoginPluginV2(dss::module_v2::LoginModuleInterfaceV2 * module, QObject *parent = nullptr);

    virtual QString icon() const override;

    virtual void setMessageCallback(MessageCallbackFunc) override;

    virtual void setAppData(void*) override;

    virtual QString message(const QString &) override;

    virtual void setAuthCallback(AuthCallbackFun) override;

    virtual void reset() override;
};

}

#endif // LOGIN_PLUGIN_V2_H
