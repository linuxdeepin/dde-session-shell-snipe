// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include <QLabel>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusPendingReply>

#include "login_module_interface_v2.h"

#include <dtkcore_global.h>
#include <DSpinner>

DCORE_BEGIN_NAMESPACE
class DConfig;
DCORE_END_NAMESPACE

namespace dss {
namespace module_v2 {

class LoginModule : public QObject
    , public LoginModuleInterfaceV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules_v2.Login" FILE "login.json")
    Q_INTERFACES(dss::module_v2::LoginModuleInterfaceV2)

    enum AuthStatus {
        None,
        Start,
        Finish
    };

public:
    explicit LoginModule(QObject *parent = nullptr);
    ~LoginModule() override;

    void init() override;
    ModuleType type() const override { return LoginType;     }
    inline QString key() const override { return QStringLiteral("one-key-login"); }
    inline QWidget *content() override { return m_loginWidget; }
    inline LoadType loadPluginType() const override { return  m_loadPluginType;}
    void reset() override;
    void setAppData(AppDataPtr) override;
    void setAuthCallback(AuthCallbackFun) override;
    void setMessageCallback(MessageCallbackFunc) override;
    QString message(const QString&) override;

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
    void stopIdentify();

private:
    AppDataPtr m_appData;
    AuthCallbackFun m_authCallback;
    MessageCallbackFunc m_messageCallback;
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
    bool m_isLocked;
    QDBusInterface*  m_login1SessionSelf;
    bool m_IdentifyWithMultipleUserStarted;
    bool m_loginAuthenticated;  // 登录界面是否已经验证过了
    int m_authFactors;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
