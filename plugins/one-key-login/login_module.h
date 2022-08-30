// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include <QLabel>

#include "login_module_interface.h"

#include "dtkcore_global.h"
#include <DSpinner>

DCORE_BEGIN_NAMESPACE
class DConfig;
DCORE_END_NAMESPACE

namespace dss {
namespace module {

class LoginModule : public QObject
    , public LoginModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Login" FILE "login.json")
    Q_INTERFACES(dss::module::LoginModuleInterface)

    enum AuthStatus {
        None,
        Start,
        Finish
    };

    enum AuthObjectType {
        LightDM,
        DeepinAuthenticate
    };

public:
    explicit LoginModule(QObject *parent = nullptr);
    ~LoginModule() override;

    void init() override;
    ModuleType type() const override { return LoginType;     }
    inline QString key() const override { return objectName(); }
    inline QWidget *content() override { return m_loginWidget; }
    inline LoadType loadPluginType() const override { return  m_loadPluginType;}
    void setCallback(LoginCallBack *callback) override;
    std::string onMessage(const std::string &) override;

public Q_SLOTS:
    void slotIdentifyStatus(const QString &name, const int errorCode, const QString &msg);
    void slotPrepareForSleep(bool active);

private:
    void initUI();
    void updateInfo();
    void initConnect();
    void startCallHuaweiFingerprint();
    void sendAuthTypeToSession(AuthType type);
    void sendAuthData(AuthCallbackData& data);

private:
    LoginCallBack *m_callback;
    AuthCallbackFun m_callbackFun;
    MessageCallbackFun m_messageCallbackFunc;
    QWidget *m_loginWidget;
    QString m_userName;
    AppType m_appType;
    LoadType m_loadPluginType;
    bool m_isAcceptFingerprintSignal;
    QTimer *m_waitAcceptSignalTimer;
    DTK_CORE_NAMESPACE::DConfig *m_dconfig;
    DTK_WIDGET_NAMESPACE::DSpinner *m_spinner;
    bool m_acceptSleepSignal;
    AuthCallbackData m_lastAuthResult;
    AuthStatus m_authStatus;
    bool m_needSendAuthType;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
