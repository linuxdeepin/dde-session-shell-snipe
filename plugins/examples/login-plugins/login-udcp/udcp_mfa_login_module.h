// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UDCP_LOGIN_MODULE_H
#define UDCP_LOGIN_MODULE_H

#include "login_module_interface_v2.h"
#include "udcp_mfa_login_widget.h"

namespace dss {
namespace module_v2 {

class UdcpMFALoginModule: public QObject,
                          public dss::module_v2::LoginModuleInterfaceV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules_v2.Login" FILE "login.json")
    Q_INTERFACES(dss::module_v2::LoginModuleInterfaceV2)
public:

    explicit UdcpMFALoginModule(QObject *parent = nullptr);
    ~UdcpMFALoginModule() noexcept override;
    QString message(const QString &msg) override;
    void reset() override;
    void init() override;
    inline void setAuthCallback(AuthCallbackFun func) override { m_authCallback = func; }
    inline void setMessageCallback(MessageCallbackFunc func) override { m_messageCallback = func; }
    inline void setAppData(AppDataPtr data) override { m_appData = data; }
    inline QWidget *content() override { return m_mainWindow; }
    inline QString key() const override { return objectName(); }
    inline QString icon() const override { return "code-block"; }
    inline ModuleType type() const override { return LoginType; }

private:
    void initUI();
    void initData();
    void initConnections();

private:

    AppDataPtr m_appData{};

    AuthCallbackFun m_authCallback{};

    MessageCallbackFunc m_messageCallback{};

    UdcpMFALoginWidget *m_mainWindow{};

    QString m_user {};

    int m_appType{};
};

}
}

#endif // UDCP_LOGIN_MODULE_H
