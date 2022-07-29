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
    inline QString key() const override { return objectName();  }
    inline QWidget *content() override { return m_loginWidget; }
    inline QString icon() const override { return "code-block"; }
    void setAppData(AppDataPtr) override;
    void setAuthCallback(AuthCallbackFunc) override;
    void setMessageCallback(MessageCallbackFunc) override;
    QString message(const QString&) override;
    void reset() override;

private:
    void initUI();
    void updateInfo();

private:
    AppDataPtr m_appData;
    AuthCallbackFunc m_authCallback;
    MessageCallbackFunc m_messageCallback;
    LoginWidget *m_loginWidget;
    QString m_userName;
    int m_appType;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
