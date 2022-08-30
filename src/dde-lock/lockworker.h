// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOCKWORKER_H
#define LOCKWORKER_H

#include "authinterface.h"
#include "dbushotzone.h"
#include "dbuslockservice.h"
#include "dbuslogin1manager.h"
#include "deepinauthframework.h"
#include "sessionbasemodel.h"
#include "switchos_interface.h"
#include "userinfo.h"

#include <QObject>
#include <QWidget>

#include <com_deepin_daemon_accounts.h>
#include <com_deepin_daemon_accounts_user.h>
#include <com_deepin_daemon_logined.h>
#include <com_deepin_sessionmanager.h>

using AccountsInter = com::deepin::daemon::Accounts;
using UserInter = com::deepin::daemon::accounts::User;
using LoginedInter = com::deepin::daemon::Logined;
using SessionManagerInter = com::deepin::SessionManager;

class SessionBaseModel;
class LockWorker : public Auth::AuthInterface
{
    Q_OBJECT
public:
    explicit LockWorker(SessionBaseModel *const model, QObject *parent = nullptr);

    void enableZoneDetected(bool disable);

    bool isLocked() const;

public slots:
    /* New authentication framework */
    void createAuthentication(const QString &account);
    void destoryAuthentication(const QString &account);
    void startAuthentication(const QString &account, const int authType);
    void endAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void onEndAuthentication(const QString &account, const int authType);

    void switchToUser(std::shared_ptr<User> user) override;
    void restartResetSessionTimer();
    void onAuthFinished();
    void onAuthStateChanged(const int type, const int state, const QString &message);

    void disableGlobalShortcutsForWayland(const bool enable);

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void doPowerAction(const SessionBaseModel::PowerAction action);
    void setCurrentUser(const std::shared_ptr<User> user);
    void setLocked(const bool locked);

    // lock
    void lockServiceEvent(quint32 eventType, quint32 pid, const QString &username, const QString &message);
    void onUnlockFinished(bool unlocked);

private:
    bool m_authenticating;
    bool m_isThumbAuth;
    DeepinAuthFramework *m_authFramework;
    DBusLockService *m_lockInter;
    DBusHotzone *m_hotZoneInter;
    QTimer *m_resetSessionTimer;
    QTimer *m_limitsUpdateTimer;
    QString m_password;
    QMap<std::shared_ptr<User>, bool> m_lockUser;
    SessionManagerInter *m_sessionManagerInter;
    HuaWeiSwitchOSInterface *m_switchosInterface = nullptr;
    bool m_canAuthenticate = false;
    QString m_account;
    QDBusInterface *m_kglobalaccelInter;
    QDBusInterface *m_kwinInter;
};

#endif // LOCKWORKER_H
