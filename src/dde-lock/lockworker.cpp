#include "authcommon.h"
#include "lockworker.h"

#include "sessionbasemodel.h"
#include "userinfo.h"

#include <DSysInfo>

#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include <grp.h>
#include <libintl.h>
#include <pwd.h>
#include <unistd.h>

#define DOMAIN_BASE_UID 10000

using namespace Auth;
using namespace AuthCommon;
DCORE_USE_NAMESPACE

LockWorker::LockWorker(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_authenticating(false)
    , m_isThumbAuth(false)
    , m_authFramework(new DeepinAuthFramework(this))
    , m_lockInter(new DBusLockService("com.deepin.dde.LockService", "/com/deepin/dde/LockService", QDBusConnection::systemBus(), this))
    , m_hotZoneInter(new DBusHotzone("com.deepin.daemon.Zone", "/com/deepin/daemon/Zone", QDBusConnection::sessionBus(), this))
    , m_resetSessionTimer(new QTimer(this))
    , m_sessionManagerInter(new SessionManagerInter("com.deepin.SessionManager", "/com/deepin/SessionManager", QDBusConnection::sessionBus(), this))
    , m_switchosInterface(new HuaWeiSwitchOSInterface("com.huawei", "/com/huawei/switchos", QDBusConnection::sessionBus(), this))
    , m_accountsInter(new AccountsInter("com.deepin.daemon.Accounts", "/com/deepin/daemon/Accounts", QDBusConnection::systemBus(), this))
    , m_loginedInter(new LoginedInter("com.deepin.daemon.Accounts", "/com/deepin/daemon/Logined", QDBusConnection::systemBus(), this))
    , m_powerInter(new PowerInter("com.deepin.daemon.Power", "/com/deepin/daemon/Power", QDBusConnection::sessionBus(), this))
{
    initConnections();
    initData();
    initConfiguration();

    m_resetSessionTimer->setInterval(15000);
    if (QGSettings::isSchemaInstalled("com.deepin.dde.session-shell")) {
         QGSettings gsetting("com.deepin.dde.session-shell", "/com/deepin/dde/session-shell/", this);
         if(gsetting.keys().contains("authResetTime")){
             int resetTime = gsetting.get("auth-reset-time").toInt();
             if(resetTime > 0)
                m_resetSessionTimer->setInterval(resetTime);
         }
    }

    m_resetSessionTimer->setSingleShot(true);
    connect(m_resetSessionTimer, &QTimer::timeout, this, [=] {
        endAuthentication(m_account, AuthTypeAll);
        destoryAuthentication(m_account);
        createAuthentication(m_account);
    });
}

LockWorker::~LockWorker()
{
}

/**
 * @brief 初始化信号连接
 */
void LockWorker::initConnections()
{
    /* com.deepin.daemon.Accounts */
    connect(m_accountsInter, &AccountsInter::UserAdded, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::addUser));
    connect(m_accountsInter, &AccountsInter::UserDeleted, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::removeUser));
    // connect(m_accountsInter, &AccountsInter::UserListChanged, m_model, &SessionBaseModel::updateUserList);  // UserListChanged信号的处理， 改用UserAdded和UserDeleted信号替代
    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, m_model, static_cast<void (SessionBaseModel::*)(const uid_t)>(&SessionBaseModel::updateLastLogoutUser));
    connect(m_loginedInter, &LoginedInter::UserListChanged, m_model, &SessionBaseModel::updateLoginedUserList);
    /* com.deepin.daemon.Authenticate */
    connect(m_authFramework, &DeepinAuthFramework::FramworkStateChanged, m_model, &SessionBaseModel::updateFrameworkState);
    connect(m_authFramework, &DeepinAuthFramework::LimitsInfoChanged, this, [this](const QString &account) {
        if (account == m_model->currentUser()->name()) {
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::SupportedEncryptsChanged, m_model, &SessionBaseModel::updateSupportedEncryptionType);
    connect(m_authFramework, &DeepinAuthFramework::SupportedMixAuthFlagsChanged, m_model, &SessionBaseModel::updateSupportedMixAuthFlags);
    /* com.deepin.daemon.Authenticate.Session */
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    connect(m_authFramework, &DeepinAuthFramework::AuthStatusChanged, this, [=](const int type, const int status, const QString &message) {
        qDebug() << "DeepinAuthFramework::AuthStatusChanged:" << type << status << message << m_model->getAuthProperty().MFAFlag;
        if (m_model->getAuthProperty().MFAFlag) {
            if (type == AuthTypeAll) {
                switch (status) {
                case StatusCodeSuccess:
                    m_model->updateAuthStatus(type, status, message);
                    destoryAuthentication(m_account);
                    onUnlockFinished(true);
                    m_resetSessionTimer->stop();
                    break;
                case StatusCodeCancel:
                    m_model->updateAuthStatus(type, status, message);
                    destoryAuthentication(m_account);
                    break;
                default:
                    break;
                }
            } else {
                switch (status) {
                case StatusCodeSuccess:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                        && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    }
                    m_resetSessionTimer->start();
                    m_model->updateAuthStatus(type, status, message);
                    break;
                case StatusCodeFailure:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                        && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    }
                    endAuthentication(m_account, type);
                    if (!m_model->currentUser()->limitsInfo(type).locked) {
                        QTimer::singleShot(50, this, [=] {
                            startAuthentication(m_account, type);
                        });
                    }
                    QTimer::singleShot(50, this, [=] {
                        m_model->updateAuthStatus(type, status, message);
                    });
                    break;
                case StatusCodeLocked:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                        && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    }
                    endAuthentication(m_account, type);
                    // TODO: 信号时序问题,考虑优化,Bug 89056
                    QTimer::singleShot(50, this, [=]{
                        m_model->updateAuthStatus(type, status, message);
                    });
                    break;
                case StatusCodeTimeout:
                case StatusCodeError:
                    endAuthentication(m_account, type);
                    m_model->updateAuthStatus(type, status, message);
                    break;
                default:
                    m_model->updateAuthStatus(type, status, message);
                    break;
                }
            }
        } else {
            switch (status) {
            case StatusCodeSuccess:
                m_model->updateAuthStatus(type, status, message);
                onUnlockFinished(true);
                if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                    && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                break;
            case StatusCodeFailure:
                m_model->updateAuthStatus(type, status, message);
                if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                    && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                if (!m_model->currentUser()->limitsInfo(AuthTypePassword).locked) {
                    startAuthentication(m_account, type);
                }
                break;
            case StatusCodeCancel:
                m_model->updateAuthStatus(type, status, message);
                destoryAuthentication(m_account);
                break;
            default:
                m_model->updateAuthStatus(type, status, message);
                break;
            }
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [=](const QString &json) {
        qDebug() << "DBusLockService::UserChanged:" << json;
        emit m_model->switchUserFinished();
        m_resetSessionTimer->stop();
    });
    connect(m_lockInter, &DBusLockService::Event, this, &LockWorker::lockServiceEvent);
    /* com.deepin.SessionManager */
    connect(m_sessionManagerInter, &SessionManagerInter::Unlock, this, [=] {
        m_authenticating = false;
        m_password.clear();
        emit m_model->authFinished(true);
    });
    /* org.freedesktop.login1.Session */
    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [=](bool active) {
        qDebug() << "DBusLockService::ActiveChanged:" << active;
        if (active) {
            createAuthentication(m_model->currentUser()->name());
        } else {
            endAuthentication(m_account, AuthTypeAll);
            destoryAuthentication(m_account);
        }
    });
    /* org.freedesktop.login1.Manager */
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [=](bool isSleep) {
        qDebug() << "DBusLogin1Manager::PrepareForSleep:" << isSleep;
        if (isSleep) {
            endAuthentication(m_account, AuthTypeAll);
            destoryAuthentication(m_account);
        } else {
            createAuthentication(m_model->currentUser()->name());
        }
        emit m_model->prepareForSleep(isSleep);
    });
    /* model */
    connect(m_model, &SessionBaseModel::authTypeChanged, this, [=](const int type) {
        if (type > 0 && !m_model->currentUser()->limitsInfo()->value(type).locked) {
            startAuthentication(m_account, m_model->getAuthProperty().AuthType);
        } else {
            QTimer::singleShot(10, this, [=] {
                m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_account));
            });
        }
    });
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, &LockWorker::doPowerAction);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [=](bool visible) {
        if (visible && m_model->currentModeState() != SessionBaseModel::ShutDownMode) {
            createAuthentication(m_model->currentUser()->name());
        } else {
            m_resetSessionTimer->stop();
            endAuthentication(m_account, AuthTypeAll);
        }
        setLocked(visible);
    });
    connect(m_model, &SessionBaseModel::onStatusChanged, this, [=](SessionBaseModel::ModeStatus status) {
        if (status == SessionBaseModel::ModeStatus::PowerMode || status == SessionBaseModel::ModeStatus::ShutDownMode) {
            checkPowerInfo();
        }
    });
    connect(m_dbusInter,&DBusObjectInter::NameOwnerChanged, this , [=](const QString &name, const QString &oldOwner, const QString &newOwner) {
        if (name == "com.deepin.daemon.Authenticate" && newOwner!= "" && m_model->visible() && m_sessionManagerInter->locked()) {
            m_resetSessionTimer->stop();
            endAuthentication(m_account, AuthTypeAll);
            createAuthentication(m_model->currentUser()->name());
        }
    });
}

void LockWorker::initData()
{
    /* com.deepin.daemon.Accounts */
    m_model->updateUserList(m_accountsInter->userList());
    m_model->updateLastLogoutUser(m_loginedInter->lastLogoutUser());
    m_model->updateLoginedUserList(m_loginedInter->userList());

    /* com.deepin.udcp.iam */
    QDBusInterface ifc("com.deepin.udcp.iam", "/com/deepin/udcp/iam", "com.deepin.udcp.iam", QDBusConnection::systemBus(), this);
    const bool allowShowCustomUser = valueByQSettings<bool>("", "loginPromptInput", false) || ifc.property("Enable").toBool();
    m_model->setAllowShowCustomUser(allowShowCustomUser);

    /* init server user or custom user */
    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || m_model->allowShowCustomUser()) {
        std::shared_ptr<User> user(new User());
        m_model->setIsServerModel(DSysInfo::deepinType() == DSysInfo::DeepinServer);
        m_model->addUser(user);
    }

    /* com.deepin.dde.LockService */
    std::shared_ptr<User> user_ptr = m_model->findUserByUid(getuid());
    if (user_ptr.get()) {
        m_model->updateCurrentUser(user_ptr);
    } else {
        m_model->updateCurrentUser(m_lockInter->CurrentUser());
    }

    /* com.deepin.daemon.Authenticate */
    m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
    m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
    m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
    m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
}

void LockWorker::initConfiguration()
{
    const bool &LockNoPasswordValue = valueByQSettings<bool>("", "lockNoPassword", false);
    m_model->setIsLockNoPassword(LockNoPasswordValue);

    m_model->setAlwaysShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Ondemand);

    checkPowerInfo();
}

void LockWorker::doPowerAction(const SessionBaseModel::PowerAction action)
{
    switch (action) {
    case SessionBaseModel::PowerAction::RequireSuspend:
        m_model->setIsBlackModel(true);
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        QTimer::singleShot(100, this, [=] {
            //为实现待机一段时间后转休眠的功能，待机调用session daemon的接口，由session daemon去做待机和转休眠的工作。
            QDBusPendingReply<> reply = m_powerInter->RequestSuspend();
            reply.waitForFinished();
            if (reply.isError()) {
                //如果调用失败则调用startdde的接口
                m_sessionManagerInter->RequestSuspend();
            }
        });
        break;
    case SessionBaseModel::PowerAction::RequireHibernate:
        m_model->setIsBlackModel(true);
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        QTimer::singleShot(100, this, [=] {
            m_sessionManagerInter->RequestHibernate();
        });
        break;
    case SessionBaseModel::PowerAction::RequireRestart:
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
            m_sessionManagerInter->RequestReboot();
        } else {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
        }
        return;
    case SessionBaseModel::PowerAction::RequireShutdown:
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
            m_sessionManagerInter->RequestShutdown();
        } else {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
        }
        return;
    case SessionBaseModel::PowerAction::RequireLock:
        m_sessionManagerInter->SetLocked(true);
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        createAuthentication(m_model->currentUser()->name());
        break;
    case SessionBaseModel::PowerAction::RequireLogout:
        m_sessionManagerInter->RequestLogout();
        return;
    case SessionBaseModel::PowerAction::RequireSwitchSystem:
        m_switchosInterface->setOsFlag(!m_switchosInterface->getOsFlag());
        QTimer::singleShot(200, this, [=] { m_sessionManagerInter->RequestReboot(); });
        break;
    case SessionBaseModel::PowerAction::RequireSwitchUser:
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
        createAuthentication(m_model->currentUser()->name());
        break;
    default:
        break;
    }

    m_model->setPowerAction(SessionBaseModel::PowerAction::None);
}

/**
 * @brief 将当前用户的信息保存到 LockService 服务
 *
 * @param user
 */
void LockWorker::setCurrentUser(const std::shared_ptr<User> user)
{
    QJsonObject json;
    json["Name"] = user->name();
    json["Type"] = user->type();
    json["Uid"] = static_cast<int>(user->uid());
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();
}

void LockWorker::switchToUser(std::shared_ptr<User> user)
{
    if (user->name() == m_account) {
        return;
    }
    qInfo() << "switch user from" << m_account << " to " << user->name() << user->isLogin();
    endAuthentication(m_account, AuthTypeAll);
    setCurrentUser(user);
    if (user->isLogin()) {
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        QProcess::startDetached("dde-switchtogreeter", QStringList());
    }
}

/**
 * @brief 设置 Locked 的状态
 *
 * @param locked
 */
void LockWorker::setLocked(const bool locked)
{
#ifndef QT_DEBUG
    if (m_model->currentModeState() != SessionBaseModel::ShutDownMode) {
        /** FIXME
         * 在执行待机操作时，后端监听的是这里设置的“Locked”，当设置为“true”时，后端认为锁屏完全起来了，执行冻结进程等接下来的操作；
         * 但是锁屏界面的显示“show”监听的是“visibleChanged”，这个信号发出后，在性能较差的机型上（arm），前端需要更长的时间来使锁屏界面显示出来，
         * 导致后端收到了“Locked=true”的信号时，锁屏界面还没有完全起来。
         * 唤醒时，锁屏接着待机前的步骤努力显示界面，但由于桌面界面在待机前一直在，不存在创建的过程，所以唤醒时直接就显示了，
         * 而这时候锁屏还在处理信号跟其它进程抢占CPU资源努力显示界面中。
         * 故增加这个延时，在待机前多给锁屏一点时间去处理显示界面的信号，尽量保证执行待机时，锁屏界面显示完成。
         * 建议后端修改监听信号或前端修改这块逻辑。
         */
        QTimer::singleShot(200, this, [=] {
            m_sessionManagerInter->SetLocked(locked);
        });
    }
#else
    Q_UNUSED(locked)
#endif
}

/**
 * @brief 旧的认证接口，已废弃
 *
 * @param password
 */
void LockWorker::authUser(const QString &password)
{
    QString userPath = m_accountsInter->FindUserByName(m_model->currentUser()->name());
    if (!userPath.startsWith("/")) {
        qWarning() << userPath;
        onDisplayErrorMsg(userPath);
        return;
    }
    m_authFramework->CreateAuthenticate(m_model->currentUser()->name());
    m_authFramework->SendToken(password);
}

void LockWorker::enableZoneDetected(bool disable)
{
    m_hotZoneInter->EnableZoneDetected(disable);
}

/**
 * @brief 显示错误信息
 *
 * @param msg
 */
void LockWorker::onDisplayErrorMsg(const QString &msg)
{
    emit m_model->authFaildTipsMessage(msg);
}

/**
 * @brief 显示提示文案
 *
 * @param msg
 */
void LockWorker::onDisplayTextInfo(const QString &msg)
{
    m_authenticating = false;
    emit m_model->authFaildMessage(msg);
}

/**
 * @brief 显示认证结果
 *
 * @param msg
 */
void LockWorker::onPasswordResult(const QString &msg)
{
    onUnlockFinished(!msg.isEmpty());

    if (msg.isEmpty()) {
        // m_authFramework->AuthenticateByUser(m_model->currentUser());
    }
}

bool LockWorker::isLocked()
{
    return m_sessionManagerInter->locked();
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void LockWorker::createAuthentication(const QString &account)
{
    qDebug() << "LockWorker::createAuthentication:" << account;
    QString userPath = m_accountsInter->FindUserByName(account);
    if (!userPath.startsWith("/")) {
        qWarning() << userPath;
        return;
    }
    m_account = account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), AppTypeLock);
        break;
    default:
        m_authFramework->CreateAuthenticate(account);
        m_model->updateFactorsInfo(MFAInfoList());
        break;
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void LockWorker::destoryAuthentication(const QString &account)
{
    qDebug() << "LockWorker::destoryAuthentication:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->DestoryAuthController(account);
        } else {
            m_authFramework->DestoryAuthController(account);
            m_authFramework->DestoryAuthenticate();
        }
        break;
    default:
        m_authFramework->DestoryAuthenticate();
        break;
    }
}

/**
 * @brief 开启认证服务    -- 作为接口提供给上层，隐藏底层细节
 *
 * @param account   账户
 * @param authType  认证类型（可传入一种或多种）
 * @param timeout   设定超时时间（默认 -1）
 */
void LockWorker::startAuthentication(const QString &account, const int authType)
{
    qDebug() << "LockWorker::startAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->StartAuthentication(account, authType, -1);
        } else {
            m_authFramework->CreateAuthenticate(account);
        }
        break;
    default:
        m_authFramework->CreateAuthenticate(account);
        break;
    }
    QTimer::singleShot(10, this, [=] {
        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
    });
}

/**
 * @brief 将密文发送给认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void LockWorker::sendTokenToAuth(const QString &account, const int authType, const QString &token)
{
    qDebug() << "LockWorker::sendTokenToAuth:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->SendTokenToAuth(account, authType, token);
        } else {
            m_authFramework->SendToken(token);
        }
        break;
    default:
        m_authFramework->SendToken(token);
        break;
    }
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 */
void LockWorker::endAuthentication(const QString &account, const int authType)
{
    qDebug() << "LockWorker::endAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->EndAuthentication(account, authType);
        break;
    default:
        break;
    }
}

void LockWorker::lockServiceEvent(quint32 eventType, quint32 pid, const QString &username, const QString &message)
{
    if (!m_model->currentUser())
        return;

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

    m_authenticating = false;

    //To Do: 最好的方案是修改同步后端认证信息的代码设计
    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::UserMode)
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

    //    if (!unlocked && m_authFramework->GetAuthType() == AuthFlag::Password) {
    //        qWarning() << "Authorization password failed!";
    //        emit m_model->authFaildTipsMessage(tr("Wrong Password"));
    //        return;
    //    }

    switch (m_model->powerAction()) {
    case SessionBaseModel::PowerAction::RequireRestart:
        if (unlocked) {
            m_sessionManagerInter->RequestReboot();
        }
        break;
    case SessionBaseModel::PowerAction::RequireShutdown:
        if (unlocked) {
            m_sessionManagerInter->RequestShutdown();
        }
        break;
    default:
        break;
    }

    emit m_model->authFinished(unlocked);
}

void LockWorker::restartResetSessionTimer()
{
    if(m_model->visible() && m_resetSessionTimer->isActive()){
        m_resetSessionTimer->start();
    }
}
