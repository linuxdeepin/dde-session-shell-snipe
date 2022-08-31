// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include "login_module_interface.h"

namespace dss {
namespace module {

class LoginModule : public QObject
    , public LoginModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Login" FILE "login.json")
    Q_INTERFACES(dss::module::LoginModuleInterface)

public:
    explicit LoginModule(QObject *parent = nullptr);
    ~LoginModule() override;

    void init() override;

    inline QString key() const override { return objectName(); }
    inline QWidget *content() override { return m_loginWidget; }
    void setCallback(LoginCallBack *callback) override;

private:
    void initUI();

private:
    LoginCallBack *m_callback;
    AuthCallbackFun m_callbackFun;
    AuthCallbackData *m_callbackData;
    QWidget *m_loginWidget;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
