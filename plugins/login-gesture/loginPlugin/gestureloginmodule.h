// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GESTURELOGINMODULE_H
#define GESTURELOGINMODULE_H

#include "login_module_interface_v2.h"

#include <QObject>
#include <QWidget>

class GesturePannel;

namespace dss {
namespace module_v2 {

class GestureLoginModule : public QObject
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

    explicit GestureLoginModule(QObject *parent = nullptr);
    ~GestureLoginModule() override;

    void init() override;
    inline QString key() const override { return objectName(); }
    QWidget *content() override;
    inline QString icon() const override { return "code-gesture"; }
    void setAppData(AppDataPtr) override;
    void setAuthCallback(AuthCallbackFun) override;
    void setMessageCallback(MessageCallbackFunc) override;
    QString message(const QString &) override;
    void reset() override;

public Q_SLOTS:
    void sendToken(const QString &);
    void enroll(const QString &);

Q_SIGNALS:
    void onAuthStateChanged(int, int);
    void onLimitsInfoChanged(bool, int, int, int, const QString &);

private:
    void initUI();
    void updateInfo();
    void initWorker();
    void updateUser(const QString &);

private:
    AppDataPtr m_appData;
    AuthCallbackFun m_authCallback;
    MessageCallbackFunc m_messageCallback;
    QPointer<QWidget> m_loginWidget;
    QString m_userName;
    int m_appType;
};

} // namespace module_v2
} // namespace dss
#endif // GESTURELOGINMODULE_H
