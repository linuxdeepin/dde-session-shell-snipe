// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
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
#include <QQueue>

#ifndef ENABLE_DSS_SNIPE
#include <com_deepin_api_soundthemeplayer.h>

using SoundThemePlayerInter = com::deepin::api::SoundThemePlayer;
#endif

class GreeterPasswordChange;

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
    void submitPasswordChange(const QString &current, const QString &password,
                              const QString &confirmation, const QString &hint);
    void cancelPasswordChange();
    void onNoPasswordLoginChanged(const QString &account, bool noPassword);

private slots:
    void onAuthStateChanged(const int type, const int state, const QString &message);
    void onReceiptChanged(bool state);
    void onCurrentUserChanged(const std::shared_ptr<User> &user);
    void onSessionCreated();
    void terminalLockedChanged(const QDBusMessage &msg);
    void resetAuth(const QString &);
    void sendExtraInfo(const QString &account, AuthCommon::AuthType authType, const QString &info);

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
    void handleAuthenticationComplete(bool result, const QString &account);
    void saveNumLockState(std::shared_ptr<User> user, bool on);
    int getNumLockState(const QString &userName);
    void recoveryUserKBState(std::shared_ptr<User> user);
    void startGreeterAuth(const QString &account = QString());
    void respondToGreeter(const QString &response);
    QString authenticationLocale() const;
    bool authenticationPageVisible() const;
    void changePasswd();
    bool ensurePasswordChangeAllowed();
    void leavePasswordChange(bool restart);
    void restorePasswordChangeUI();
    void scheduleAuthenticationRestart();
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
    QString m_authenticationLocale;
    QQueue<QPair<QString, QLightDM::Greeter::PromptType>> m_unansweredGreeterPrompts;
    QString m_password;
    bool m_retryAuth;
    bool m_restartGreeterAuth = false;
    bool m_passwordVerified = false;
    bool m_directPamPassword = false;
    GreeterPasswordChange *m_passwordChange;
    bool m_discardGreeterCompletion = false;
    bool m_restartAfterCancel = false;
    QString m_savedAuthAccount;
    AuthFlags m_savedAuthType = AT_None;
    bool m_savedMFA = false;
};

#endif  // GREETERWORKEK_H
