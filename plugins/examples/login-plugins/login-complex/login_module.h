// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include "login_module_interface_v2.h"
#include "login-widget.h"

namespace dss {
namespace module_v2 {

class LoginModule : public QObject
    , public LoginModuleInterfaceV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules_v2.Login" FILE "login.json")
    Q_INTERFACES(dss::module_v2::LoginModuleInterfaceV2)

public:

    enum DefaultAuthLevel {
        NoDefault = 0,
        Default,
        StrongDefault
    };

    explicit LoginModule(QObject *parent = nullptr);
    ~LoginModule() override;

    void init() override;
    inline QString key() const override { return objectName();  }
    inline QWidget *content() override { return m_loginWidget; }
    inline QString icon() const override { return "code-block"; }
    void setAppData(AppDataPtr) override;
    void setAuthCallback(AuthCallbackFun) override;
    void setMessageCallback(MessageCallbackFunc) override;
    QString message(const QString&) override;
    void reset() override;

private:
    void initUI();
    void updateInfo();

private:
    AppDataPtr m_appData;
    AuthCallbackFun m_authCallback;
    MessageCallbackFunc m_messageCallback;
    LoginWidget *m_loginWidget;
    QString m_userName;
    int m_appType;
};

} // namespace module_v2
} // namespace dss
#endif // LOGIN_MODULE_H
