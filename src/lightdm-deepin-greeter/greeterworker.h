// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include "authinterface.h"
#include "dbuslockservice.h"
#include "dbuslogin1manager.h"
#include "deepinauthframework.h"
#include "sessionbasemodel.h"

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QObject>

#ifndef ENABLE_DSS_SNIPE
#include <com_deepin_api_soundthemeplayer.h>

using SoundThemePlayerInter = com::deepin::api::SoundThemePlayer;
#endif

class GreeterWorker : public Auth::AuthInterface
{
    Q_OBJECT
public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    explicit GreeterWorker(SessionBaseModel *const model, QObject *parent = nullptr);
    ~GreeterWorker() override;

    void switchToUser(std::shared_ptr<User> user) override;
    bool isSecurityEnhanceOpen();

signals:
    void requestUpdateBackground(const QString &path);
    void requestShowPrompt(const QString &prompt);
    void requestShowMessage(const QString &message);
    void requestShowUsersWithTheSameName(const QString nativeUserName, const QString &doMainAccountDetail);

public slots:
    /* New authentication framework */
    void createAuthentication(const QString &account);
    void destroyAuthentication(const QString &account);
    void startAuthentication(const QString &account, const AuthFlags authType);
    void endAuthentication(const QString &account, const AuthFlags authType);
    void sendTokenToAuth(const QString &account, const AuthType authType, const QString &token);

    void checkAccount(const QString &account, bool switchUser);
    void checkSameNameAccount(const QString &account, bool switchUser);
    void restartResetSessionTimer();
    void onAuthFinished();
    void onNoPasswordLoginChanged(const QString &account, bool noPassword);

private slots:
    void onAuthStateChanged(const int type, const int state, const QString &message);
    void onReceiptChanged(bool state);
    void onCurrentUserChanged(const std::shared_ptr<User> &user);
    void onSessionCreated();
    void terminalLockedChanged(const QDBusMessage &msg);
    void resetAuth(const QString &);

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void doPowerAction(const SessionBaseModel::PowerAction action);
    void setCurrentUser(const std::shared_ptr<User> user);

    void checkDBusServer(bool isValid);
    void showPrompt(const QString &text, const QLightDM::Greeter::PromptType type);
    void showMessage(const QString &text, const QLightDM::Greeter::MessageType type);
    void authenticationComplete();
    void saveNumLockState(std::shared_ptr<User> user, bool on);
    int getNumLockState(const QString &userName);
    void recoveryUserKBState(std::shared_ptr<User> user);
    void startGreeterAuth(const QString &account = QString());
    void changePasswd();
    void screenSwitchByWldpms(bool active);
    void updatePasswordExpiredStateBySPName(const QString &account);
#ifdef ENABLE_DSS_SNIPE
    void prepareShutdownSound();
#endif

private:
    DeepinAuthFramework *m_authFramework;
    QLightDM::Greeter *m_greeter;
    DBusLockService *m_lockInter;
    QDBusInterface *m_systemDaemon;
#ifndef ENABLE_DSS_SNIPE
    SoundThemePlayerInter *m_soundPlayerInter;
#endif
    QTimer *m_resetSessionTimer;
    QTimer *m_limitsUpdateTimer;
    QString m_account;
    QString m_password;
    bool m_retryAuth;
};

#endif  // GREETERWORKEK_H
