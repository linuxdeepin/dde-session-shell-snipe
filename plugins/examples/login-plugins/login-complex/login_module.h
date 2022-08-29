// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include "login_module_interface.h"
#include "login-widget.h"

namespace dss {
namespace module {

class LoginModule : public QObject
    , public LoginModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Login" FILE "login.json")
    Q_INTERFACES(dss::module::LoginModuleInterface)

public:

    enum DefaultAuthLevel {
        NoDefault = 0,
        Default,
        StrongDefault
    };

    explicit LoginModule(QObject *parent = nullptr);
    ~LoginModule() override;

    void init() override;
    ModuleType type() const override { return LoginType;     }
    inline QString key() const override { return objectName();  }
    inline QWidget *content() override { return m_loginWidget; }
    inline std::string icon() const override { return "code-block"; }
    void setCallback(LoginCallBack *callback) override;
    std::string onMessage(const std::string &) override;

private:
    void initUI();
    void updateInfo();

private:
    LoginCallBack *m_callback;
    AuthCallbackFun m_authCallbackFun;
    MessageCallbackFun m_messageCallbackFunc;
    LoginWidget *m_loginWidget;
    QString m_userName;
    int m_appType;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
