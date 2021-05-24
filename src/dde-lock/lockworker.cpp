#include "lockworker.h"

#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/userinfo.h"

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <libintl.h>
#include <QDebug>
#include <QApplication>
#include <QProcess>
#include <QRegularExpression>
#include <DSysInfo>
#include <pwd.h>

#include <com_deepin_daemon_power.h>

#define LOCKSERVICE_PATH "/com/deepin/dde/LockService"
#define LOCKSERVICE_NAME "com.deepin.dde.LockService"

using PowerInter = com::deepin::daemon::Power;
using namespace Auth;
DCORE_USE_NAMESPACE

LockWorker::LockWorker(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_authenticating(false)
    , m_isThumbAuth(false)
    , m_lockInter(new DBusLockService(LOCKSERVICE_NAME, LOCKSERVICE_PATH, QDBusConnection::systemBus(), this))
    , m_hotZoneInter(new DBusHotzone("com.deepin.daemon.Zone", "/com/deepin/daemon/Zone", QDBusConnection::sessionBus(), this))
    , m_sessionManager(new SessionManager("com.deepin.SessionManager", "/com/deepin/SessionManager", QDBusConnection::sessionBus(), this))
{
    m_currentUserUid = getuid();
    m_authFramework = new DeepinAuthFramework(this, this);
    m_sessionManager->setSync(false);

    //当前用户m_currentUserUid是已登录用户,直接按AuthInterface::onLoginUserListChanged中的流程处理
    std::shared_ptr<User> u(new ADDomainUser(m_currentUserUid));
    u->setisLogind(true);
    struct passwd *pws;
    pws = getpwuid(m_currentUserUid);
    static_cast<ADDomainUser *>(u.get())->setUserDisplayName(pws->pw_name);
    static_cast<ADDomainUser *>(u.get())->setUserName(pws->pw_name);
    m_model->userAdd(u);
    m_model->setCurrentUser(u);

    //该信号用来处理初始化切换用户(锁屏+锁屏)或者切换用户(锁屏+登陆)两种种场景的指纹认证
    connect(m_lockInter, &DBusLockService::UserChanged, this, &LockWorker::onCurrentUserChanged);

    connect(model, &SessionBaseModel::onPowerActionChanged, this, [ = ](SessionBaseModel::PowerAction poweraction) {
        switch (poweraction) {
        case SessionBaseModel::PowerAction::RequireSuspend:
            m_sessionManager->RequestSuspend();
            break;
        case SessionBaseModel::PowerAction::RequireHibernate:
            m_sessionManager->RequestHibernate();
            break;
        case SessionBaseModel::PowerAction::RequireRestart:
            m_authFramework->Authenticate(m_model->currentUser());
            model->setPowerAction(SessionBaseModel::PowerAction::RequireRestart);
            return;
        case SessionBaseModel::PowerAction::RequireShutdown:
            m_authFramework->Authenticate(m_model->currentUser());
            model->setPowerAction(SessionBaseModel::PowerAction::RequireShutdown);
            return;
        default:
            break;
        }

        model->setPowerAction(SessionBaseModel::PowerAction::None);
    });

    connect(model, &SessionBaseModel::lockLimitFinished, this, [ = ] {
        auto user = m_model->currentUser();
        if (user != nullptr && !user->isLock()) {
            m_password.clear();
            m_authFramework->Authenticate(user);
        }
    });

    connect(model, &SessionBaseModel::visibleChanged, this, [ = ](bool isVisible) {
        if (!isVisible || model->currentType() != SessionBaseModel::AuthType::LockType) return;

        std::shared_ptr<User> user_ptr = model->currentUser();
        if (!user_ptr.get()) return;

        if (user_ptr->type() == User::ADDomain && user_ptr->uid() == 0) return;

        m_authFramework->Authenticate(user_ptr);
    });

    connect(m_lockInter, &DBusLockService::Event, this, &LockWorker::lockServiceEvent);
    connect(model, &SessionBaseModel::onStatusChanged, this, [ = ](SessionBaseModel::ModeStatus status) {
        if (status == SessionBaseModel::ModeStatus::PowerMode) {
            checkPowerInfo();
        }
    });

    connect(m_sessionManager, &SessionManager::Unlock, this, [ = ] {
        m_authenticating = false;
        m_password.clear();
        emit m_model->authFinished(true);
    });

    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [ = ](bool active) {
        if (!active) {
            m_canAuthenticate = true;
        } else if(m_canAuthenticate) {
            m_canAuthenticate = false;
            m_authenticating = false;
            m_authFramework->Authenticate(m_model->currentUser());
        }
    });

    //因为部分机器在待机休眠唤醒后无法发出Login1SessionSelf::ActiveChange信号，从而无法请求验证密码，增加此信号连接重新请求验证
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [ = ](bool sleep) {
        if (sleep) {
            m_canAuthenticate = true;
        } else if(m_canAuthenticate) {
            m_canAuthenticate = false;
            m_authenticating = false;
            m_authFramework->Authenticate(m_model->currentUser());
        }
    });

    const bool &LockNoPasswordValue { valueByQSettings<bool>("", "lockNoPassword", false) };
    m_model->setIsLockNoPassword(LockNoPasswordValue);

    m_model->setAlwaysShowUserSwitchButton(true);
    m_model->setAllowShowUserSwitchButton(true);

    {
        initDBus();
        initData();
    }

    QDBusInterface interface("com.deepin.udcp.iam",
                             "/com/deepin/udcp/iam",
                             "com.deepin.udcp.iam",
                             QDBusConnection::systemBus());
    if (interface.isValid()) {
        //INT_MAX这个值远程账号可能会使用，参考lightdm改用系统平常用不到的UID 999
        std::shared_ptr<User> user = std::make_shared<ADDomainUser>(DEFAULT_ENTRY_UID);
        static_cast<ADDomainUser *>(user.get())->setUserDisplayName("...");
        static_cast<ADDomainUser *>(user.get())->setIsServerUser(true);
        m_model->userAdd(user);
    }
}

void LockWorker::switchToUser(std::shared_ptr<User> user)
{
    qWarning() << "switch user from" << m_model->currentUser()->name() << " to " << user->name();

    // clear old password
    m_password.clear();
    m_authenticating = false;

    // if type is lock, switch to greeter
    QJsonObject json;
    json["Uid"] = static_cast<int>(user->uid());
    json["Type"] = user->type();

    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();

    if (user->isLogin()) {
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        QProcess::startDetached("dde-switchtogreeter");
    }
}

void LockWorker::authUser(const QString &password)
{
    if (m_authenticating) return;

    m_authenticating = true;

    // auth interface
    std::shared_ptr<User> user = m_model->currentUser();

    m_password = password;

    qWarning() << "start authentication of user: " << user->name();

    // 服务器登录输入用户与当前用户不同时给予提示
    if (m_currentUserUid != user->uid()) {
        QTimer::singleShot(800, this, [ = ] {
            onUnlockFinished(false);
        });
        return;
    }

    if(!m_authFramework->isAuthenticate())
        m_authFramework->Authenticate(user);

    m_authFramework->Responsed(password);
}

void LockWorker::enableZoneDetected(bool disable)
{
    m_hotZoneInter->EnableZoneDetected(disable);
}

void LockWorker::onDisplayErrorMsg(const QString &msg)
{
    emit m_model->authFaildTipsMessage(msg);
}

void LockWorker::onDisplayTextInfo(const QString &msg)
{
    m_authenticating = false;
    emit m_model->authFaildMessage(msg);
}

void LockWorker::onPasswordResult(const QString &msg)
{
    onUnlockFinished(!msg.isEmpty());

    if(msg.isEmpty()) {
        m_authFramework->Authenticate(m_model->currentUser());
    }
}

void LockWorker::onUserAdded(const QString &user)
{
    std::shared_ptr<User> user_ptr = nullptr;
    uid_t uid = user.mid(QString(ACCOUNTS_DBUS_PREFIX).size()).toUInt();
    if(uid != DEFAULT_ENTRY_UID) {
        user_ptr = std::make_shared<NativeUser>(user);
    } else {
        user_ptr = std::make_shared<ADDomainUser>(uid);
        static_cast<ADDomainUser *>(user_ptr.get())->setUserName(userPwdName(uid));
    }

    if (!user_ptr->isUserIsvalid())
        return;

    user_ptr->setisLogind(isLogined(user_ptr->uid()));

    if (user_ptr->uid() == m_currentUserUid) {
        m_model->setCurrentUser(user_ptr);

        // 正常情况认证走SessionBaseModel::visibleChanged,这里是异常状况没有触发认证的补充,Authenticate调用时间间隔过短,会导致认证会崩溃,加延时处理
        QTimer::singleShot(100, user_ptr.get(), [ = ]{
            if (user_ptr.get()) {
                m_authFramework->Authenticate(user_ptr);
            }
        });
    }

    if (user_ptr->uid() == m_lastLogoutUid) {
        m_model->setLastLogoutUser(user_ptr);
    }

    m_model->userAdd(user_ptr);
}

void LockWorker::lockServiceEvent(quint32 eventType, quint32 pid, const QString &username, const QString &message)
{
    if (!m_model->currentUser()) return;

    if (username != m_model->currentUser()->name())
        return;

    // Don't show password prompt from standard pam modules since
    // we'll provide our own prompt or just not.
    const QString msg = message.simplified() == "Password:" ? "" : message;

    m_authenticating = false;

    if (msg == "Verification timed out") {
        m_isThumbAuth = true;
        emit m_model->authFaildMessage(tr("Fingerprint verification timed out, please enter your password manually"));
        return;
    }

    switch (eventType) {
    case DBusLockService::PromptQuestion:
        qWarning() << "prompt quesiton from pam: " << message;
        emit m_model->authFaildMessage(message);
        break;
    case DBusLockService::PromptSecret:
        qWarning() << "prompt secret from pam: " << message;
        if (m_isThumbAuth && !msg.isEmpty()) {
            emit m_model->authFaildMessage(msg);
        }
        break;
    case DBusLockService::ErrorMsg:
        qWarning() << "error message from pam: " << message;
        if (msg == "Failed to match fingerprint") {
            emit m_model->authFaildTipsMessage(tr("Failed to match fingerprint"));
            emit m_model->authFaildMessage("");
        }
        break;
    case DBusLockService::TextInfo:
        emit m_model->authFaildMessage(QString(dgettext("fprintd", message.toLatin1())));
        break;
    case DBusLockService::Failure:
        onUnlockFinished(false);
        break;
    case DBusLockService::Success:
        onUnlockFinished(true);
        break;
    default:
        break;
    }
}

void LockWorker::onUnlockFinished(bool unlocked)
{
    qWarning() << "LockWorker::onUnlockFinished -- unlocked status : " << unlocked;
    emit m_model->authFinished(unlocked);

    m_authenticating = false;

    if (!unlocked && m_authFramework->GetAuthType() == AuthFlag::Password) {
        qWarning() << "Authorization password failed!";
        emit m_model->authFaildTipsMessage(tr("Wrong Password"));
        return;
    }

    switch (m_model->powerAction()) {
    case SessionBaseModel::PowerAction::RequireRestart:
        m_model->setPowerAction(SessionBaseModel::PowerAction::RequireRestart);
        if (unlocked) {
            m_sessionManager->RequestReboot();
        }
        return;
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_model->setPowerAction(SessionBaseModel::PowerAction::RequireShutdown);
        if (unlocked) {
            m_sessionManager->RequestShutdown();
        }
        return;
    default:
        break;
    }
}

void LockWorker::onCurrentUserChanged(const QString &user)
{
    qWarning() << "LockWorker::onCurrentUserChanged -- change to :" << user;
    const QJsonObject obj = QJsonDocument::fromJson(user.toUtf8()).object();
    auto user_cur = static_cast<uint>(obj["Uid"].toInt());
    if (user_cur == m_currentUserUid) {
        for (std::shared_ptr<User> user_ptr : m_model->userList()) {
            if (user_ptr->uid() == m_currentUserUid) {
                m_authFramework->Authenticate(user_ptr);
                break;
            }
        }
    }
    emit m_model->switchUserFinished();
}
