#ifndef LOCKWORKER_H
#define LOCKWORKER_H

#include "authinterface.h"
#include "dbushotzone.h"
#include "dbuslockservice.h"
#include "dbuslogin1manager.h"
#include "deepinauthframework.h"
#include "interface/deepinauthinterface.h"
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
    , public DeepinAuthInterface
{
    Q_OBJECT
public:
    explicit LockWorker(SessionBaseModel *const model, QObject *parent = nullptr);
    ~LockWorker();

    void enableZoneDetected(bool disable);

    /* Old authentication methods */
    void onDisplayErrorMsg(const QString &msg) override;
    void onDisplayTextInfo(const QString &msg) override;
    void onPasswordResult(const QString &msg) override;
    //获取当前Session是否被锁定
    bool isLocked();

public slots:
    /* Old authentication methods */
    void authUser(const QString &password) override;
    /* New authentication framework */
    void createAuthentication(const QString &account);
    void destoryAuthentication(const QString &account);
    void startAuthentication(const QString &account, const int authType);
    void endAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);

    void switchToUser(std::shared_ptr<User> user) override;
    void setLocked(const bool locked);
    void restartResetSessionTimer();

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void doPowerAction(const SessionBaseModel::PowerAction action);
    void setCurrentUser(const std::shared_ptr<User> user);

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
    QString m_password;
    QMap<std::shared_ptr<User>, bool> m_lockUser;
    SessionManagerInter *m_sessionManagerInter;
    HuaWeiSwitchOSInterface *m_switchosInterface = nullptr;
    bool m_canAuthenticate = false;
    AccountsInter *m_accountsInter;
    LoginedInter *m_loginedInter;
    QString m_account;
};

#endif // LOCKWORKER_H
