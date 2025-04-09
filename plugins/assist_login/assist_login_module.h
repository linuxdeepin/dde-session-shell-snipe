// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASSIST_LOGIN_MODULE_H
#define ASSIST_LOGIN_MODULE_H

#include "login_module_interface.h"
#include "assist_login_work.h"
#include "assist_login_widget.h"

namespace dss {
namespace module {

class AssistLoginModule: public QObject, public LoginModuleInterface
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Login" FILE "login.json")
    Q_INTERFACES(dss::module::LoginModuleInterface)

public:
    explicit AssistLoginModule(QObject *parent = nullptr);
    ~AssistLoginModule() override;

    void init() override;
    ModuleType type() const override { return IpcAssistLoginType; }
    inline QString key() const override { return objectName(); }
    inline QWidget *content() override { return (QWidget *) m_mainWidget; }
    void setCallback(LoginCallBack *callback) override;
    std::string onMessage(const std::string &) override;
    void reset() override;


private:
    void initUI();
    void initConnect();

private Q_SLOTS:
    void sendAuthToken(const QString &user, const QString &passwd);

private:
    LoginCallBack *m_callback;
    AuthCallbackFun m_callbackFun;
    AuthCallbackData *m_callbackData;
    MessageCallbackFun m_messageCallbackFunc;
    AssistLoginWork *m_work;
    AssistLoginWidget *m_mainWidget;
};

}
}
#endif //ASSIST_LOGIN_MODULE_H
