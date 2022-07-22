/*
 * Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     Fang Shichao <fangshichao@uniontech.com>
 *
 * Maintainer: Fang Shichao <fangshichao@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include <QLabel>

#include "login_module_interface.h"

#include "dtkcore_global.h"
#include "public_func.h"
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
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
