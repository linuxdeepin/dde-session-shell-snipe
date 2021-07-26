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

const QString AuthenticateService("com.deepin.daemon.Authenticate");

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
    , m_authFramework(new DeepinAuthFramework(this, this))
    , m_lockInter(new DBusLockService(LOCKSERVICE_NAME, LOCKSERVICE_PATH, QDBusConnection::systemBus(), this))
    , m_xEventInter(new XEventInter("com.deepin.api.XEventMonitor","/com/deepin/api/XEventMonitor",QDBusConnection::sessionBus(), this))
    , m_resetSessionTimer(new QTimer(this))
    , m_authenticating(false)
{
    if (!isConnectSync()) {
        qWarning() << "greeter connect fail !!!";
    }

    initConnections();

    const QString &switchUserButtonValue {valueByQSettings<QString>("Lock", "showSwitchUserButton", "ondemand")};
    m_model->setAlwaysShowUserSwitchButton(switchUserButtonValue == "always");
    m_model->setAllowShowUserSwitchButton(switchUserButtonValue == "ondemand");

    {
        initDBus();
        initData();

        if (QFile::exists("/etc/deepin/no_suspend"))
            m_model->setCanSleep(false);

        checkDBusServer(m_accountsInter->isValid());
    }

    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || valueByQSettings<bool>("", "loginPromptInput", false)) {
        std::shared_ptr<User> user = std::make_shared<ADDomainUser>(INT_MAX);
        static_cast<ADDomainUser *>(user.get())->setUserDisplayName("...");
        static_cast<ADDomainUser *>(user.get())->setIsServerUser(true);
        m_model->setIsServerModel(true);
        m_model->userAdd(user);
        m_model->setCurrentUser(user);
    } else {
        connect(m_login1Inter, &DBusLogin1Manager::SessionRemoved, this, [=] {
            // lockservice sometimes fails to call on olar server
            QDBusPendingReply<QString> replay = m_lockInter->CurrentUser();
            replay.waitForFinished();

            if (!replay.isError()) {
                const QJsonObject obj = QJsonDocument::fromJson(replay.value().toUtf8()).object();
                auto user_ptr = m_model->findUserByUid(static_cast<uint>(obj["Uid"].toInt()));

                m_model->setCurrentUser(user_ptr);
                // userAuthForLightdm(user_ptr);
            }
        });
    }
    m_model->setCurrentUser(m_lockInter->CurrentUser());

    /* com.deepin.daemon.Accounts */
    m_model->updateUserList(m_accountsInter->userList());
    m_model->updateLoginedUserList(m_loginedInter->userList());
    m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
    /* com.deepin.daemon.Authenticate */
    m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
    m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
    m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());

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
    connect(m_resetSessionTimer,&QTimer::timeout,this,[ = ]{
         destoryAuthentication(m_account);
         createAuthentication(m_account);
    });
    m_xEventInter->RegisterFullScreen();
}

GreeterWorkek::~GreeterWorkek()
{
    delete m_greeter;
    delete m_authFramework;
    delete m_lockInter;
}

void GreeterWorkek::initConnections()
{
    /* greeter */
    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &GreeterWorkek::showPrompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &GreeterWorkek::showMessage);
    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &GreeterWorkek::authenticationComplete);
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
        if (m_model->getAuthProperty().MFAFlag) {
            if (type == AuthTypeAll && status == StatusCodeSuccess) {
                m_resetSessionTimer->stop();
                if (m_greeter->inAuthentication()) {
                    m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
                    m_model->updateAuthStatus(type, status, message);
                } else {
                    qWarning() << "The lightdm is not in authentication!";
                }

            } else if (type != AuthTypeAll) {
                switch (status) {
                case StatusCodeSuccess:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    m_resetSessionTimer->start();
                    endAuthentication(m_account, type);
                    m_model->updateAuthStatus(type, status, message);
                    break;
                case StatusCodeFailure:
                case StatusCodeLocked:
                    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
                        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                    endAuthentication(m_account, type);
                    QTimer::singleShot(10, this, [=] {
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
        if (m_model->currentUser() == nullptr || m_model->currentUser()->name().isEmpty()) {
            return;
        }
        if (active) {
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswdGrp()) {
                createAuthentication(m_model->currentUser()->name());
            }
            if (!m_model->isServerModel() && m_model->currentUser()->isNoPasswdGrp() && m_model->currentUser()->automaticLogin()) {
                m_greeter->authenticate(m_model->currentUser()->name());
            }
        } else {
            destoryAuthentication(m_account);
        }
    });
    /* org.freedesktop.login1.Manager */
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [=](bool isSleep) {
        if (isSleep) {
            destoryAuthentication(m_account);
        } else {
            createAuthentication(m_account);
        }
        // emit m_model->prepareForSleep(isSleep);
    });
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [=] (const QString &json) {
        m_resetSessionTimer->stop();
        destoryAuthentication(m_account);
        m_model->setCurrentUser(json);
        std::shared_ptr<User> user_ptr = m_model->currentUser();
        const QString &account = user_ptr->name();
        if (!user_ptr.get()->isLogin() && !user_ptr.get()->isNoPasswdGrp()) {
            createAuthentication(account);
        }
        if (user_ptr.get()->isNoPasswdGrp()) {
            emit m_model->authTypeChanged(AuthTypeNone);
        }
        emit m_model->switchUserFinished();
    });

    connect(m_xEventInter, &XEventInter::CursorMove, this, [=] {
        if(m_model->visible() && m_resetSessionTimer->isActive()){
            m_resetSessionTimer->start();
        }
    });

    connect(m_xEventInter, &XEventInter::KeyRelease, this, [=] {
        if(m_model->visible() && m_resetSessionTimer->isActive()){
            m_resetSessionTimer->start();
        }
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
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswdGrp()) {
                createAuthentication(m_model->currentUser()->name());
            }
            if (!m_model->isServerModel() && m_model->currentUser()->isNoPasswdGrp() && m_model->currentUser()->automaticLogin()) {
                m_greeter->authenticate(m_model->currentUser()->name());
            }
        } else {
            m_resetSessionTimer->stop();
        }
    });
    /* others */
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::numlockStatusChanged, this, [=] (bool on) {
        saveNumlockStatus(m_model->currentUser(), on);
    });

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

void GreeterWorkek::switchToUser(std::shared_ptr<User> user)
{
    if (user->uid() == INT_MAX) {
        emit m_model->authTypeChanged(AuthType::AuthTypeNone);
    }

    if (user->name() == m_account) {
        return;
    }

    qInfo() << "switch user from" << m_account << " to " << user->name() << user->isLogin();

    QJsonObject json;
    json["Uid"] = static_cast<int>(user->uid());
    json["Type"] = user->type();

    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();

    // just switch user
    if (user->isLogin()) {
        // switch to user Xorg
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        m_model->setCurrentUser(user);
    }
}

/**
 * @brief 旧的认证接口，已废弃
 *
 * @param password
 */
void GreeterWorkek::authUser(const QString &password)
{
    if (m_authenticating)
        return;

    m_authenticating = true;

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

void GreeterWorkek::onUserAdded(const QString &user)
{
    std::shared_ptr<NativeUser> user_ptr(new NativeUser(user));
    user_ptr->setisLogind(isLogined(user_ptr->uid()));

    if (m_model->currentUser().get() == nullptr) {
        if (m_model->userList().isEmpty() || m_model->userList().first()->type() == User::ADDomain) {
            m_model->setCurrentUser(user_ptr);
        }
    }

    if (!user_ptr->isLogin() && user_ptr->uid() == m_currentUserUid && !m_model->isServerModel()) {
        m_model->setCurrentUser(user_ptr);
        // userAuthForLightdm(user_ptr);
    }

    if (user_ptr->uid() == m_lastLogoutUid) {
        m_model->setLastLogoutUser(user_ptr);
    }

    connect(user_ptr->getUserInter(), &UserInter::UserNameChanged, this, [=] {
        if (!user_ptr->isNoPasswdGrp()) {
            updateLockLimit(user_ptr);
        }
    });

    m_model->userAdd(user_ptr);
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void GreeterWorkek::createAuthentication(const QString &account)
{
    m_account = account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case 0:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), AppTypeLogin);
        m_authFramework->SetAuthQuitFlag(account, DeepinAuthFramework::ManualQuit);
        break;
    default:
        m_model->updateFactorsInfo(MFAInfoList());
        break;
    }
    if (m_model->getAuthProperty().MFAFlag) {
        if (!m_authFramework->SetPrivilegesEnable(account, QString("/usr/sbin/lightdm"))) {
            qWarning() << "Failed to set privileges!";
        }
        m_greeter->authenticate(account);
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void GreeterWorkek::destoryAuthentication(const QString &account)
{
    switch (m_model->getAuthProperty().FrameworkState) {
    case 0:
        m_authFramework->SetPrivilegesDisable(account);
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
    switch (m_model->getAuthProperty().FrameworkState) {
    case 0:
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
    //密码输入类型
    if(authType == 1)
      m_password = token;

    switch (m_model->getAuthProperty().FrameworkState) {
    case 0:
        if (m_model->getAuthProperty().MFAFlag) {
            m_authFramework->SendTokenToAuth(account, authType, token);
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
    m_authFramework->EndAuthentication(account, authType);
}

/**
 * @brief 检查用户输入的账户是否合理
 *
 * @param account
 */
void GreeterWorkek::checkAccount(const QString &account)
{
    QString userPath = m_accountsInter->FindUserByName(account);
    if (!userPath.startsWith("/") && !account.startsWith("@")) {
        qWarning() << userPath;
        userPath = tr("Wrong account");
        onDisplayErrorMsg(userPath);
        return;
    }
    std::shared_ptr<User> user_ptr = std::make_shared<NativeUser>(userPath);
    m_model->setCurrentUser(user_ptr);
    if (user_ptr->isNoPasswdGrp()) {
        m_greeter->authenticate(account);
    } else {
        m_resetSessionTimer->stop();
        destoryAuthentication(account);
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
                m_model->setCurrentUser(user_ptr);
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
    if (user.get() != nullptr && !user->isNoPasswdGrp()) {
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
    switch (type) {
    case QLightDM::Greeter::PromptTypeSecret:
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
    switch (type) {
    case QLightDM::Greeter::MessageTypeInfo:
        m_model->updateAuthStatus(AuthTypeSingle, StatusCodeSuccess, text);
        break;
    case QLightDM::Greeter::MessageTypeError:
        m_model->updateAuthStatus(AuthTypeSingle, StatusCodeFailure, text);
        break;
    }
}

/**
 * @brief 认证完成
 */
void GreeterWorkek::authenticationComplete()
{
    qInfo() << "authentication complete, authenticated " << m_greeter->isAuthenticated();

    if (!m_greeter->isAuthenticated()) {
        // m_authenticating = false;
        // if (m_password.isEmpty()) {
        //     resetLightdmAuth(m_model->currentUser(), 100, false);
        //     return;
        // }

        // m_password.clear();

        if (m_model->currentUser()->type() == User::Native) {
            emit m_model->authFaildTipsMessage(tr("Wrong Password"));
        }

        if (m_model->currentUser()->type() == User::ADDomain) {
            emit m_model->authFaildTipsMessage(tr("The account or password is not correct. Please enter again."));
        }

        // resetLightdmAuth(m_model->currentUser(), 100, false);

        return;
    }

    emit m_model->authFinished(m_greeter->isAuthenticated());

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

    auto startSessionSync = [=] () {
        QJsonObject json;
        json["Uid"] = static_cast<int>(m_model->currentUser()->uid());
        json["Type"] = m_model->currentUser()->type();
        m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();

        m_greeter->startSessionSync(m_model->sessionKey());
        m_authenticating = false;
    };

    // NOTE(kirigaya): It is not necessary to display the login animation.
    connect(m_model->currentUser().get(), &User::desktopBackgroundPathChanged, this, &GreeterWorkek::requestUpdateBackground);
    m_model->currentUser()->desktopBackgroundPath();

#ifndef DISABLE_LOGIN_ANI
    QTimer::singleShot(1000, this, startSessionSync);
#else
    startSessionSync();
#endif
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
    //m_authenticating = false;
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
    if (user->isLock()) {
        return;
    }

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
