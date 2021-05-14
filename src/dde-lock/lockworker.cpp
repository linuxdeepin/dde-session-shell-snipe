#include "lockworker.h"

#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/userinfo.h"
#include "src/session-widgets/framedatabind.h"

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <libintl.h>
#include <QDebug>
#include <QApplication>
#include <QProcess>
#include <QRegularExpression>
#include <DSysInfo>

#include <syslog.h>

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

    //该信号用来处理初始化lock、锁屏或者切换用户(锁屏+登陆)三种场景的指纹认证
    QObject::connect(model, &SessionBaseModel::visibleChanged, this, [ = ](bool visible){
        qDebug() << "SessionBaseModel::visibleChanged -- visible status :" << visible;
        auto user = m_model->currentUser();
        if(visible && user->uid() == m_currentUserUid) {
            syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
            m_authFramework->Authenticate(user);
        }
    });
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
            syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
            m_authFramework->Authenticate(m_model->currentUser());
            model->setPowerAction(SessionBaseModel::PowerAction::RequireRestart);
            return;
        case SessionBaseModel::PowerAction::RequireShutdown:
            syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
            m_authFramework->Authenticate(m_model->currentUser());
            model->setPowerAction(SessionBaseModel::PowerAction::RequireShutdown);
            return;
        default:
            break;
        }

        model->setPowerAction(SessionBaseModel::PowerAction::None);
    });

    connect(model, &SessionBaseModel::lockChanged, this, [ = ](bool lock) {
        if (!lock) {
            syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
            m_authFramework->Authenticate(m_model->currentUser());
        }
    });

    connect(model, &SessionBaseModel::visibleChanged, this, [ = ](bool isVisible) {
        if (!isVisible || model->currentType() != SessionBaseModel::AuthType::LockType) return;

        std::shared_ptr<User> user_ptr = model->currentUser();
        if (!user_ptr.get()) return;

        if (user_ptr->type() == User::ADDomain && user_ptr->uid() == 0) return;

        //userAuthForLock(user_ptr);
    });

    connect(m_lockInter, &DBusLockService::Event, this, &LockWorker::lockServiceEvent);
    connect(model, &SessionBaseModel::onStatusChanged, this, [ = ](SessionBaseModel::ModeStatus status) {
        if (status == SessionBaseModel::ModeStatus::PowerMode) {
            checkPowerInfo();
        }
    });

    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, this, &LockWorker::onLastLogoutUserChanged);
    connect(m_loginedInter, &LoginedInter::UserListChanged, this, &LockWorker::onLoginUserListChanged);

    connect(m_sessionManager, &SessionManager::Unlock, this, [ = ] {
        qDebug() << "SessionManager::Unlock";
        m_authenticating = false;
        m_authFramework->CancelCurrentAuth();
    });

    const bool &LockNoPasswordValue { valueByQSettings<bool>("", "lockNoPassword", false) };
    m_model->setIsLockNoPassword(LockNoPasswordValue);

    const QString &switchUserButtonValue { valueByQSettings<QString>("Lock", "showSwitchUserButton", "ondemand") };
    m_model->setAlwaysShowUserSwitchButton(switchUserButtonValue == "always");
    m_model->setAllowShowUserSwitchButton(switchUserButtonValue == "ondemand");

    {
        initDBus();
        initData();
    }

    // init ADDomain User
    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || valueByQSettings<bool>("", "loginPromptInput", false)) {
        std::shared_ptr<User> user = std::make_shared<ADDomainUser>(INT_MAX);
        static_cast<ADDomainUser *>(user.get())->setUserDisplayName("...");
        static_cast<ADDomainUser *>(user.get())->setIsServerUser(true);
        m_model->setIsServerModel(true);
        m_model->userAdd(user);
    }
}

void LockWorker::switchToUser(std::shared_ptr<User> user)
{
    qDebug() << "switch user from" << m_model->currentUser()->name() << " to " << user->name();

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
    syslog(LOG_INFO, "zl: %s %d m_authenticating %d ", __func__, __LINE__, m_authenticating);
    if (m_authenticating) return;

    m_authenticating = true;

    // auth interface
    std::shared_ptr<User> user = m_model->currentUser();
    qDebug() << "LockWorker::authUser: start authentication for" << user->name();

    // 服务器登录输入用户与当前用户不同时给予提示
    if (m_currentUserUid != user->uid()) {
        QTimer::singleShot(800, this, [ = ] {
            onUnlockFinished(false, false);
        });
        return;
    }

    if (!m_authFramework->Responsed(password)) {
        m_authenticating = false;
        emit m_model->authFinished(false);
    }
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
    emit m_model->authFaildMessage(msg);
    FrameDataBind::Instance()->updateValue("deepinAuthMsg", msg);
}

void LockWorker::onPasswordResult(const QString &msg)
{
    syslog(LOG_INFO, "zl: %s %d msg %s", __func__, __LINE__, msg.toStdString().c_str());
    qDebug() << "LockWorker onPasswordResult: " << msg;
    bool unlocked = (msg == "succes");
    onUnlockFinished(unlocked, true);
}

void LockWorker::onUserAdded(const QString &user)
{
    std::shared_ptr<User> user_ptr(new NativeUser(user));
    if (!user_ptr->isUserIsvalid())
        return;

    user_ptr->setisLogind(isLogined(user_ptr->uid()));

    if (user_ptr->uid() == m_currentUserUid) {
        m_model->setCurrentUser(user_ptr);
    }

    if (user_ptr->uid() == m_lastLogoutUid) {
        syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
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

    if (msg == "Verification timed out") {
        m_isThumbAuth = true;
        emit m_model->authFaildMessage(tr("Fingerprint verification timed out, please enter your password manually"));
        onUnlockFinished(false, false);
        return;
    }

    switch (eventType) {
    case DBusLockService::PromptQuestion:
        qDebug() << "prompt quesiton from pam: " << message;
        emit m_model->authFaildMessage(message);
        onUnlockFinished(false, false);
        break;
    case DBusLockService::PromptSecret:
        qDebug() << "prompt secret from pam: " << message;
        if (m_isThumbAuth && !msg.isEmpty()) {
            emit m_model->authFaildMessage(msg);
        }
        onUnlockFinished(false, false);
        break;
    case DBusLockService::ErrorMsg:
        qWarning() << "error message from pam: " << message;
        if (msg == "Failed to match fingerprint") {
            emit m_model->authFaildTipsMessage(tr("Failed to match fingerprint"));
            emit m_model->authFaildMessage("");
        }
        onUnlockFinished(false, false);
        break;
    case DBusLockService::TextInfo:
        qDebug() << "DBusLockService::TextInfo";
        emit m_model->authFaildMessage(QString(dgettext("fprintd", message.toLatin1())));
        onUnlockFinished(false, false);
        break;
    case DBusLockService::Failure:
        qDebug() << "DBusLockService::Failure";
        onUnlockFinished(false, false);
        break;
    case DBusLockService::Success:
        qDebug() << "DBusLockService::Success";
        onUnlockFinished(true, false);
        break;
    default:
        break;
    }
}

void LockWorker::onUnlockFinished(bool unlocked, bool fromAgent)
{
    syslog(LOG_INFO, "zl: %s %d unlocked %d from agent %d", __func__, __LINE__, unlocked, fromAgent);
    qDebug() << "LockWorker::onUnlockFinished -- unlocked =" << unlocked << ", fromAgent =" << fromAgent;
    if (unlocked) {
        m_model->currentUser()->resetLock();
        switch (m_model->powerAction()) {
        case SessionBaseModel::PowerAction::RequireRestart:
            m_model->setPowerAction(SessionBaseModel::PowerAction::RequireRestart);
            if (unlocked) {
                m_sessionManager->RequestReboot();
            }
            break;//重启关机，也还是要先把锁屏界面移走，因为有时候会有程序阻止重启关机
        case SessionBaseModel::PowerAction::RequireShutdown:
            m_model->setPowerAction(SessionBaseModel::PowerAction::RequireShutdown);
            if (unlocked) {
                m_sessionManager->RequestShutdown();
            }
            break;//重启关机，也还是要先把锁屏界面移走，因为有时候会有程序阻止重启关机
        default:
            break;
        }
    }
    qDebug() << "LockWorker::onUnlockFinished -- emit authFinished";
    emit m_model->authFinished(unlocked);
    m_authFramework->CancelCurrentAuth();

    //验校密码中
    bool isLockForNum = false;
    syslog(LOG_INFO, "zl: %s %d is authenticating %d ", __func__, __LINE__, m_authenticating);
    if (m_authenticating) {
        m_authenticating = false;
        if (!unlocked) {
            if (m_authFramework->GetAuthType() == AuthFlag::Password && fromAgent) {
//                qDebug() << "LockWorker::onUnlockFinished: Authorization password failed!";
                syslog(LOG_INFO, "zl: %s %d Authorization password failed!", __func__, __LINE__);
                emit m_model->authFaildTipsMessage(tr("Wrong Password"));
                isLockForNum = m_model->currentUser()->isLockForNum();
                if (isLockForNum) {
                    m_model->currentUser()->startLock();
                }
            }

            if (!isLockForNum) {
                syslog(LOG_INFO, "zl: %s %d start new authentication", __func__, __LINE__);
//                qDebug() << "LockWorker::onUnlockFinished -- start new auth for other failed reason";
                m_authFramework->Authenticate(m_model->currentUser());
            }
        }
    } else if (!m_authenticating && !unlocked && !isLockForNum){
        m_authFramework->Authenticate(m_model->currentUser());
    }
}

void LockWorker::onCurrentUserChanged(const QString &user)
{
    qDebug() << "LockWorker::onCurrentUserChanged -- change to :" << user;
    const QJsonObject obj = QJsonDocument::fromJson(user.toUtf8()).object();
    auto user_cur = static_cast<uint>(obj["Uid"].toInt());
    if (user_cur == m_currentUserUid) {
        for (std::shared_ptr<User> user_ptr : m_model->userList()) {
            if (user_ptr->uid() == m_currentUserUid) {
                syslog(LOG_INFO, "zl: %s %d ", __func__, __LINE__);
                m_authFramework->Authenticate(user_ptr);
                return;
            }
        }
    } else {
        return;
    }
}
