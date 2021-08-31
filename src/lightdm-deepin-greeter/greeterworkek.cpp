#include "authcommon.h"
#include "greeterworkek.h"
#include "userinfo.h"
#include "keyboardmonitor.h"

#include <unistd.h>
#include <libintl.h>
#include <DSysInfo>

#include <QGSettings>

#include <com_deepin_system_systempower.h>

#define LOCKSERVICE_PATH "/com/deepin/dde/LockService"
#define LOCKSERVICE_NAME "com.deepin.dde.LockService"

using PowerInter = com::deepin::system::Power;
using namespace Auth;
using namespace AuthCommon;
DCORE_USE_NAMESPACE

class UserNumlockSettings
{
public:
    UserNumlockSettings(const QString &username)
        : m_username(username)
        , m_settings(QSettings::UserScope, "deepin", "greeter")
    {
    }

    int get(const int defaultValue) { return m_settings.value(m_username, defaultValue).toInt(); }
    void set(const int value) { m_settings.setValue(m_username, value); }

private:
    QString m_username;
    QSettings m_settings;
};

GreeterWorkek::GreeterWorkek(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_greeter(new QLightDM::Greeter(this))
    , m_authFramework(new DeepinAuthFramework(this))
    , m_lockInter(new DBusLockService(LOCKSERVICE_NAME, LOCKSERVICE_PATH, QDBusConnection::systemBus(), this))
    , m_resetSessionTimer(new QTimer(this))
{
#ifndef QT_DEBUG
    if (!m_greeter->connectSync()) {
        qCritical() << "greeter connect fail !!!";
        exit(1);
    }
#endif

    checkDBusServer(m_accountsInter->isValid());

    initConnections();
    initData();
    initConfiguration();

    //认证超时重启
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
        m_model->updateAuthStatus(AuthTypeAll, StatusCodeCancel, "Cancel");
        destoryAuthentication(m_account);
        createAuthentication(m_account);
    });
}

GreeterWorkek::~GreeterWorkek()
{
}

void GreeterWorkek::initConnections()
{
    /* greeter */
    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &GreeterWorkek::showPrompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &GreeterWorkek::showMessage);
    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &GreeterWorkek::authenticationComplete);
    /* com.deepin.daemon.Accounts */
    connect(m_accountsInter, &AccountsInter::UserAdded, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::addUser));
    connect(m_accountsInter, &AccountsInter::UserDeleted, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::removeUser));
    // connect(m_accountsInter, &AccountsInter::UserListChanged, m_model, &SessionBaseModel::updateUserList); // UserListChanged信号的处理， 改用UserAdded和UserDeleted信号替代
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
    connect(m_authFramework, &DeepinAuthFramework::AuthStatusChanged, this, [=](const int type, const int status, const QString &message) {
        qDebug() << "DeepinAuthFramework::AuthStatusChanged:" << type << status << message;
        if (m_model->getAuthProperty().MFAFlag) {
            if (type == AuthTypeAll) {
                switch (status) {
                case StatusCodeSuccess:
                    m_model->updateAuthStatus(type, status, message);
                    m_resetSessionTimer->stop();
                    if (m_greeter->inAuthentication()) {
                        m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
                    } else {
                        qWarning() << "The lightdm is not in authentication!";
                    }
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
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    m_resetSessionTimer->start();
                    m_model->updateAuthStatus(type, status, message);
                    break;
                case StatusCodeFailure:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode) {
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
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    endAuthentication(m_account, type);
                    // TODO: 信号时序问题,考虑优化,Bug 89056
                    QTimer::singleShot(50, this, [=] {
                        m_model->updateAuthStatus(type, status, message);
                    });
                    break;
                case StatusCodeTimeout:
                case StatusCodeError:
                    m_model->updateAuthStatus(type, status, message);
                    endAuthentication(m_account, type);
                    break;
                default:
                    m_model->updateAuthStatus(type, status, message);
                    break;
                }
            }
        } else {
            if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode && (status == StatusCodeSuccess || status == StatusCodeFailure))
                m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
            m_model->updateAuthStatus(type, status, message);
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    /* org.freedesktop.login1.Session */
    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [=](bool active) {
        qDebug() << "Login1SessionSelf::ActiveChanged:" << active;
        if (m_model->currentUser() == nullptr || m_model->currentUser()->name().isEmpty()) {
            return;
        }
        if (active) {
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            }
            if (!m_model->isServerModel() && m_model->currentUser()->isNoPasswordLogin() && m_model->currentUser()->isAutomaticLogin()) {
                m_greeter->authenticate(m_model->currentUser()->name());
            }
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
    });
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [=](const QString &json) {
        qDebug() << "DBusLockService::UserChanged:" << json;
        m_resetSessionTimer->stop();
        m_model->updateCurrentUser(json);
        std::shared_ptr<User> user_ptr = m_model->currentUser();
        const QString &account = user_ptr->name();
        if (user_ptr.get()->isNoPasswordLogin()) {
            emit m_model->authTypeChanged(AuthTypeNone);
            m_account = account;
        }
        emit m_model->switchUserFinished();
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
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, &GreeterWorkek::doPowerAction);
    connect(m_model, &SessionBaseModel::lockLimitFinished, this, [=] {
        if (!m_greeter->inAuthentication()) {
            m_greeter->authenticate(m_account);
        }
        startAuthentication(m_account, m_model->getAuthProperty().AuthType);
    });
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &GreeterWorkek::recoveryUserKBState);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [=] (bool visible) {
        if (visible) {
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            m_resetSessionTimer->stop();
        }
    });
    /* others */
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::numlockStatusChanged, this, [=](bool on) {
        saveNumlockStatus(m_model->currentUser(), on);
    });
}

void GreeterWorkek::initData()
{
    /* com.deepin.daemon.Accounts */
    m_model->updateUserList(m_accountsInter->userList());
    m_model->updateLastLogoutUser(m_loginedInter->lastLogoutUser());
    m_model->updateLoginedUserList(m_loginedInter->userList());

    /* com.deepin.udcp.iam */
    QDBusInterface ifc("com.deepin.udcp.iam", "/com/deepin/udcp/iam", "com.deepin.udcp.iam", QDBusConnection::systemBus(), this);
    const bool allowShowCustomUser = valueByQSettings<bool>("", "loginPromptInput", false) || ifc.property("Enable").toBool();
    m_model->setAllowShowCustomUser(allowShowCustomUser);

    /* init cureent user */
    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || m_model->allowShowCustomUser()) {
        std::shared_ptr<User> user(new User());
        m_model->setIsServerModel(DSysInfo::deepinType() == DSysInfo::DeepinServer);
        m_model->addUser(user);
        if (DSysInfo::deepinType() == DSysInfo::DeepinServer) {
            m_model->updateCurrentUser(user);
        }
    } else {
        connect(m_login1Inter, &DBusLogin1Manager::SessionRemoved, this, [=] {
            qDebug() << "DBusLogin1Manager::SessionRemoved";
            // lockservice sometimes fails to call on olar server
            QDBusPendingReply<QString> replay = m_lockInter->CurrentUser();
            replay.waitForFinished();

            if (!replay.isError()) {
                const QJsonObject obj = QJsonDocument::fromJson(replay.value().toUtf8()).object();
                auto user_ptr = m_model->findUserByUid(static_cast<uint>(obj["Uid"].toInt()));

                m_model->updateCurrentUser(user_ptr);
            }
        });
    }

    /* com.deepin.dde.LockService */
    m_model->updateCurrentUser(m_lockInter->CurrentUser());

    /* com.deepin.daemon.Authenticate */
    m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
    m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
    m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
    m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
}

void GreeterWorkek::initConfiguration()
{
    m_model->setAlwaysShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Ondemand);

    checkPowerInfo();

    if (QFile::exists("/etc/deepin/no_suspend")) {
        m_model->setCanSleep(false);
    }

    //当这个配置不存在是，如果是不是笔记本就打开小键盘，否则就关闭小键盘 0关闭键盘 1打开键盘 2默认值（用来判断是不是有这个key）
    if (m_model->currentUser() != nullptr && UserNumlockSettings(m_model->currentUser()->name()).get(2) == 2) {
        PowerInter powerInter("com.deepin.system.Power", "/com/deepin/system/Power", QDBusConnection::systemBus(), this);
        if (powerInter.hasBattery()) {
            saveNumlockStatus(m_model->currentUser(), 0);
        } else {
            saveNumlockStatus(m_model->currentUser(), 1);
        }
        recoveryUserKBState(m_model->currentUser());
    }
}

void GreeterWorkek::doPowerAction(const SessionBaseModel::PowerAction action)
{
    switch (action) {
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        break;
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        break;
    case SessionBaseModel::PowerAction::RequireSuspend:
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        m_login1Inter->Suspend(true);
        break;
    case SessionBaseModel::PowerAction::RequireHibernate:
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        m_login1Inter->Hibernate(true);
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
void GreeterWorkek::setCurrentUser(const std::shared_ptr<User> user)
{
    QJsonObject json;
    json["Name"] = user->name();
    json["Type"] = user->type();
    json["Uid"] = static_cast<int>(user->uid());
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();
}

void GreeterWorkek::switchToUser(std::shared_ptr<User> user)
{
    if (user->name() == m_account) {
        return;
    }
    qInfo() << "switch user from" << m_account << " to " << user->name() << user->uid() << user->isLogin();
    endAuthentication(m_account, AuthTypeAll);

    if (user->uid() == INT_MAX) {
        m_greeter->authenticate();
        m_model->setAuthType(AuthTypeNone);
    }
    setCurrentUser(user);
    if (user->isLogin()) { // switch to user Xorg
        m_greeter->authenticate();
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        m_model->updateAuthStatus(AuthTypeAll, StatusCodeCancel, "Cancel");
        destoryAuthentication(m_account);
        m_model->updateCurrentUser(user);
        if (!user->isNoPasswordLogin()) {
            createAuthentication(user->name());
        }
    }
}

/**
 * @brief 旧的认证接口，已废弃
 *
 * @param password
 */
void GreeterWorkek::authUser(const QString &password)
{
    // auth interface
    std::shared_ptr<User> user = m_model->currentUser();
    m_password = password;

    qWarning() << "greeter authenticate user: " << m_greeter->authenticationUser() << " current user: " << user->name();
    if (m_greeter->authenticationUser() != user->name()) {
        resetLightdmAuth(user, 100, false);
    } else {
        if (m_greeter->inAuthentication()) {
            // m_authFramework->AuthenticateByUser(user);
            // m_authFramework->Responsed(password);
            // m_greeter->respond(password);
        } else {
            m_greeter->authenticate(user->name());
        }
    }
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void GreeterWorkek::createAuthentication(const QString &account)
{
    qDebug() << "GreeterWorkek::createAuthentication:" << account;
    m_account = account;
    if (account.isEmpty()) {
        m_model->setAuthType(AuthTypeNone);
        return;
    }
    m_retryAuth = false;
    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    if (user_ptr) {
        user_ptr->updatePasswordExpiredInfo();
    }
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), AppTypeLogin);
        m_authFramework->SetAuthQuitFlag(account, DeepinAuthFramework::ManualQuit);
        if (m_model->getAuthProperty().MFAFlag) {
            if (!m_authFramework->SetPrivilegesEnable(account, QString("/usr/sbin/lightdm"))) {
                qWarning() << "Failed to set privileges!";
            }
            m_greeter->authenticate(account);
        }
        break;
    default:
        m_model->updateFactorsInfo(MFAInfoList());
        break;
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void GreeterWorkek::destoryAuthentication(const QString &account)
{
    qDebug() << "GreeterWorkek::destoryAuthentication:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->DestoryAuthController(account);
        break;
    default:
        break;
    }
}

/**
 * @brief 开启认证服务    -- 作为接口提供给上层，屏蔽底层细节
 *
 * @param account   账户
 * @param authType  认证类型（可传入一种或多种）
 * @param timeout   设定超时时间（默认 -1）
 */
void GreeterWorkek::startAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorkek::startAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->StartAuthentication(account, authType, -1);
        } else {
            m_greeter->authenticate(account);
        }
        break;
    default:
        m_greeter->authenticate(account);
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
void GreeterWorkek::sendTokenToAuth(const QString &account, const int authType, const QString &token)
{
    qDebug() << "GreeterWorkek::sendTokenToAuth:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->SendTokenToAuth(account, authType, token);
            if (authType == AuthTypePassword) {
                m_password = token; // 用于解锁密钥环
            }
        } else {
            m_greeter->respond(token);
        }
        break;
    default:
        m_greeter->respond(token);
        break;
    }
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 */
void GreeterWorkek::endAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorkek::endAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (authType == AuthTypeAll) {
            m_authFramework->SetPrivilegesDisable(account);
        }
        m_authFramework->EndAuthentication(account, authType);
        break;
    default:
        break;
    }
}

/**
 * @brief 检查用户输入的账户是否合理
 *
 * @param account
 */
void GreeterWorkek::checkAccount(const QString &account)
{
    qDebug() << "GreeterWorkek::checkAccount:" << account;
    if (m_greeter->authenticationUser() == account) {
        return;
    }
    const QString userPath = m_accountsInter->FindUserByName(account);
    std::shared_ptr<User> user_ptr;
    if (!userPath.startsWith("/")) {
        if (account.startsWith("@")) {
            user_ptr = std::make_shared<ADDomainUser>(INT_MAX - 1);
            dynamic_cast<ADDomainUser *>(user_ptr.get())->setName(account);
            dynamic_cast<ADDomainUser *>(user_ptr.get())->setFullName(account.midRef(QString("@").size()).toString());
        } else {
            qWarning() << userPath;
            onDisplayErrorMsg(tr("Wrong account"));
            m_model->setAuthType(AuthTypeNone);
            m_greeter->authenticate();
            return;
        }
    } else {
        user_ptr = std::make_shared<NativeUser>(userPath);
    }
    m_model->updateCurrentUser(user_ptr);
    if (user_ptr->isNoPasswordLogin()) {
        m_greeter->authenticate(account);
    } else {
        m_resetSessionTimer->stop();
        endAuthentication(m_account, AuthTypeAll);
        m_model->updateAuthStatus(AuthTypeAll, StatusCodeCancel, "Cancel");
        destoryAuthentication(m_account);
        createAuthentication(account);
    }
}

void GreeterWorkek::checkDBusServer(bool isvalid)
{
    if (isvalid) {
        m_accountsInter->userList();
    } else {
        // FIXME: 我不希望这样做，但是QThread::msleep会导致无限递归
        QTimer::singleShot(300, this, [=] {
            qWarning() << "com.deepin.daemon.Accounts is not start, rechecking!";
            checkDBusServer(m_accountsInter->isValid());
        });
    }
}

void GreeterWorkek::oneKeyLogin()
{
    // 多用户一键登陆
    QDBusPendingCall call = m_authenticateInter->PreOneKeyLogin(AuthFlag::Fingerprint);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [=] {
        if (!call.isError()) {
            QDBusReply<QString> reply = call.reply();
            qWarning() << "one key Login User Name is : " << reply.value();

            auto user_ptr = m_model->findUserByName(reply.value());
            if (user_ptr.get() != nullptr && reply.isValid()) {
                m_model->updateCurrentUser(user_ptr);
                userAuthForLightdm(user_ptr);
            }
        } else {
            qWarning() << "pre one key login: " << call.error().message();
        }

        watcher->deleteLater();
    });
}

void GreeterWorkek::userAuthForLightdm(std::shared_ptr<User> user)
{
    if (user.get() != nullptr && !user->isNoPasswordLogin()) {
        //后端需要大约600ms时间去释放指纹设备
        resetLightdmAuth(user, 100, true);
    }
}

/**
 * @brief 显示提示信息
 *
 * @param text
 * @param type
 */
void GreeterWorkek::showPrompt(const QString &text, const QLightDM::Greeter::PromptType type)
{
    qDebug() << "GreeterWorkek::showPrompt:" << text << type;
    switch (type) {
    case QLightDM::Greeter::PromptTypeSecret:
        m_retryAuth = true;
        m_model->updateAuthStatus(AuthTypeSingle, StatusCodePrompt, text);
        break;
    case QLightDM::Greeter::PromptTypeQuestion:
        break;
    }
}

/**
 * @brief 显示认证成功/失败的信息
 *
 * @param text
 * @param type
 */
void GreeterWorkek::showMessage(const QString &text, const QLightDM::Greeter::MessageType type)
{
    qDebug() << "GreeterWorkek::showMessage:" << text << type;
    switch (type) {
    case QLightDM::Greeter::MessageTypeInfo:
        m_model->updateAuthStatus(AuthTypeSingle, StatusCodeSuccess, text);
        break;
    case QLightDM::Greeter::MessageTypeError:
        if (m_retryAuth && m_model->getAuthProperty().MFAFlag) {
            m_model->updateMFAFlag(false);
            m_model->setAuthType(AuthTypeSingle);
        }
        m_retryAuth = false;
        m_model->updateAuthStatus(AuthTypeSingle, StatusCodeFailure, text);
        break;
    }
}

/**
 * @brief 认证完成
 */
void GreeterWorkek::authenticationComplete()
{
    const bool result = m_greeter->isAuthenticated();
    qInfo() << "Authentication result:" << result << m_retryAuth;

    if (!result) {
        if (m_retryAuth && !m_model->getAuthProperty().MFAFlag) {
            showMessage(tr("Wrong Password"), QLightDM::Greeter::MessageTypeError);
        }
        if (!m_model->currentUser()->limitsInfo(AuthTypePassword).locked) {
            m_greeter->authenticate(m_model->currentUser()->name());
        }
        return;
    }

    emit m_model->authFinished(result);

    m_password.clear();

    switch (m_model->powerAction()) {
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        return;
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        return;
    default:
        break;
    }

    qInfo() << "start session = " << m_model->sessionKey();

    auto startSessionSync = [=]() {
        setCurrentUser(m_model->currentUser());
        m_greeter->startSessionSync(m_model->sessionKey());
    };

#ifndef DISABLE_LOGIN_ANI
    QTimer::singleShot(1000, this, startSessionSync);
#else
    startSessionSync();
#endif
    endAuthentication(m_account, AuthTypeAll);
    destoryAuthentication(m_account);
}

void GreeterWorkek::saveNumlockStatus(std::shared_ptr<User> user, const bool &on)
{
    UserNumlockSettings(user->name()).set(on);
}

void GreeterWorkek::recoveryUserKBState(std::shared_ptr<User> user)
{
    //FIXME(lxz)
    //    PowerInter powerInter("com.deepin.system.Power", "/com/deepin/system/Power", QDBusConnection::systemBus(), this);
    //    const BatteryPresentInfo info = powerInter.batteryIsPresent();
    //    const bool defaultValue = !info.values().first();
    if (user.get() == nullptr)
        return;

    const bool enabled = UserNumlockSettings(user->name()).get(false);

    qWarning() << "restore numlock status to " << enabled;

    // Resync numlock light with numlock status
    bool cur_numlock = KeyboardMonitor::instance()->isNumlockOn();
    KeyboardMonitor::instance()->setNumlockStatus(!cur_numlock);
    KeyboardMonitor::instance()->setNumlockStatus(cur_numlock);

    KeyboardMonitor::instance()->setNumlockStatus(enabled);
}

//TODO 显示内容
void GreeterWorkek::onDisplayErrorMsg(const QString &msg)
{
    emit m_model->authFaildTipsMessage(msg);
}

void GreeterWorkek::onDisplayTextInfo(const QString &msg)
{
    emit m_model->authFaildMessage(msg);
}

void GreeterWorkek::onPasswordResult(const QString &msg)
{
    //onUnlockFinished(!msg.isEmpty());

    //if(msg.isEmpty()) {
    //    m_authFramework->AuthenticateByUser(m_model->currentUser());
    //}
}

void GreeterWorkek::resetLightdmAuth(std::shared_ptr<User> user, int delay_time, bool is_respond)
{
    QTimer::singleShot(delay_time, this, [=] {
        // m_greeter->authenticate(user->name());
        // m_authFramework->AuthenticateByUser(user);
        if (is_respond && !m_password.isEmpty()) {
            // if (m_framworkState == 0) {
            //其他
            // } else if (m_framworkState == 1) {
            //没有修改过
            // m_authFramework->Responsed(m_password);
            // } else if (m_framworkState == 2) {
            //修改过的pam
            // m_greeter->respond(m_password);
            // }
        }
    });
}

void GreeterWorkek::restartResetSessionTimer()
{
    if(m_model->visible() && m_resetSessionTimer->isActive()){
        m_resetSessionTimer->start();
    }
}
