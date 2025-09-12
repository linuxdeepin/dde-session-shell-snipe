// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeterworker.h"

#include "authcommon.h"
#include "keyboardmonitor.h"
#include "userinfo.h"
#include "dconfig_helper.h"
#include "login_plugin_util.h"
#include "mfasequencecontrol.h"
#include "signal_bridge.h"

#include <DSysInfo>

#ifndef ENABLE_DSS_SNIPE
#include <dde-api/eventlogger.hpp>

#include <QGSettings>

#include <com_deepin_system_systempower.h>
#else
#include "systempower1interface.h"
#endif
#include <pwd.h>
#include </usr/include/shadow.h>

#define SECURITY_ENHANCE_PATH "/com/deepin/daemon/SecurityEnhance"
#define SECURITY_ENHANCE_NAME "com.deepin.daemon.SecurityEnhance"

#ifndef ENABLE_DSS_SNIPE
using PowerInter = com::deepin::system::Power;
#else
using PowerInter = org::deepin::dde::Power1;
#endif
using namespace Auth;
using namespace AuthCommon;
DCORE_USE_NAMESPACE

const int NUM_LOCKED = 0;   // 禁用小键盘
const int NUM_UNLOCKED = 1; // 启用小键盘
const int NUM_LOCK_UNKNOWN = 2; // 未知状态

GreeterWorker::GreeterWorker(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_authFramework(new DeepinAuthFramework(this))
    , m_greeter(new QLightDM::Greeter(this))
    , m_lockInter(new DBusLockService(DSS_DBUS::lockService, DSS_DBUS::lockServicePath, QDBusConnection::systemBus(), this))
    , m_systemDaemon(new QDBusInterface(DSS_DBUS::systemDaemonService, DSS_DBUS::systemDaemonPath, DSS_DBUS::systemDaemonService, QDBusConnection::systemBus(), this))
#ifndef ENABLE_DSS_SNIPE
    , m_soundPlayerInter(new SoundThemePlayerInter("com.deepin.api.SoundThemePlayer", "/com/deepin/api/SoundThemePlayer", QDBusConnection::systemBus(), this))
#endif
    , m_resetSessionTimer(new QTimer(this))
    , m_limitsUpdateTimer(new QTimer(this))
    , m_retryAuth(false)
{
#ifndef QT_DEBUG
    if (!m_greeter->connectSync()) {
        qCritical() << "ERROR: greeter connect fail!";
        exit(1);
    }
#endif
    checkDBusServer(m_accountsInter->isValid());

    initConnections();
    initData();
    initConfiguration();

    m_limitsUpdateTimer->setSingleShot(true);
    m_limitsUpdateTimer->setInterval(50);

    //认证超时重启
    m_resetSessionTimer->setInterval(15000);
#ifndef ENABLE_DSS_SNIPE
    if (m_gsettings != nullptr && m_gsettings->keys().contains("authResetTime")) {
        int resetTime = m_gsettings->get("auth-reset-time").toInt();
        if (resetTime > 0)
            m_resetSessionTimer->setInterval(resetTime);
    }
#else
    if (!m_dConfig) {
        m_dConfig = DConfig::create("org.deepin.dde.session-shell", "org.deepin.dde.session-shell", QString(), this);
    }
    int resetTime = m_dConfig->value("authResetTime", 15000).toInt();
    if (resetTime > 0) {
        m_resetSessionTimer->setInterval(resetTime);
    }
#endif

    m_resetSessionTimer->setSingleShot(true);
    connect(m_resetSessionTimer, &QTimer::timeout, this, [this] {
        qCInfo(DDE_SHELL) << "Reset session time out";
        endAuthentication(m_account, AT_All);
        m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
        destroyAuthentication(m_account);
        createAuthentication(m_account);
    });
}

GreeterWorker::~GreeterWorker()
{
}

void GreeterWorker::initConnections()
{
    /* greeter */
    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &GreeterWorker::showPrompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &GreeterWorker::showMessage);
    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &GreeterWorker::authenticationComplete);
    /* com.deepin.daemon.Accounts */
    connect(m_accountsInter, &AccountsInter::UserAdded, this, [this](const QString &path) {
        // Account接口中的path可能被不同用户复用，导致model中数据未更新
        qCInfo(DDE_SHELL) << "User add, path:" << path;
        auto userPtr = m_model->findUserByPath(path);
        if (userPtr) {
            auto userName = userPtr->name();
            QString accountPath = m_accountsInter->FindUserByName(userName);
            if (accountPath != path) {
                // 查询结果空或者变动
                qCWarning(DDE_SHELL) << "users updated, force remove : " << path << userName;
                m_model->removeUser(path);
            }
        }
        m_model->addUser(path);
    });
    connect(m_accountsInter, &AccountsInter::UserDeleted, this, [this](const QString &path) {
        qCInfo(DDE_SHELL) << "User delete, path:" << path;
        // Account接口中的path可能被不同用户复用，导致model中数据未更新
        // 先remove
        m_model->removeUser(path);
        m_model->updateUserList(m_accountsInter->userList());

        if (path == m_model->currentUser()->path()) {
            if (!m_model->updateCurrentUser(m_lockInter->CurrentUser())) {
                qCWarning(DDE_SHELL) << "updateCurrentUser failed";
            }
            m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
            destroyAuthentication(m_account);
            if (!m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            } else {
                m_model->setAuthType(AT_None);
            }
#ifndef ENABLE_DSS_SNIPE
            m_soundPlayerInter->PrepareShutdownSound(static_cast<int>(m_model->currentUser()->uid()));
#else
            prepareShutdownSound();
#endif
        }
    });
    connect(m_loginedInter, &LoginedInter::UserListChanged, m_model, &SessionBaseModel::updateLoginedUserList);
    /* com.deepin.daemon.Authenticate */
    connect(m_authFramework, &DeepinAuthFramework::FramworkStateChanged, m_model, &SessionBaseModel::updateFrameworkState);
    connect(m_authFramework, &DeepinAuthFramework::LimitsInfoChanged, this, [this](const QString &account) {
        qDebug() << "GreeterWorker::initConnections LimitsInfoChanged:" << account;
        if (account == m_model->currentUser()->name()) {
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::DeviceChanged, this, [this](const int type, const int state) {
        qCInfo(DDE_SHELL) << "Device changed, type: " << type << ", state: " << state;
        // 如果是单因或者多因且包含该类型认证，需要重新创建认证
        if (m_model->visible() && (!m_model->getAuthProperty().MFAFlag || (m_model->getAuthProperty().MFAFlag && (m_model->getAuthProperty().AuthType & type)))) {
            endAuthentication(m_account, AT_All);
            destroyAuthentication(m_account);
            createAuthentication(m_account);
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::SupportedEncryptsChanged, m_model, &SessionBaseModel::updateSupportedEncryptionType);
    connect(m_authFramework, &DeepinAuthFramework::SupportedMixAuthFlagsChanged, m_model, &SessionBaseModel::updateSupportedMixAuthFlags);
    /* com.deepin.daemon.Authenticate.Session */
    connect(m_authFramework, &DeepinAuthFramework::AuthStateChanged, this, &GreeterWorker::onAuthStateChanged);
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    connect(m_authFramework, &DeepinAuthFramework::SessionCreated, this, &GreeterWorker::onSessionCreated);
    connect(m_authFramework, &DeepinAuthFramework::TokenAccountMismatch, this, &GreeterWorker::resetAuth);

    /* org.freedesktop.login1.Session */
    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [this](bool active) {
        qCInfo(DDE_SHELL) << "Login1SessionSelf::ActiveChanged:" << active;
        if (m_model->currentUser() == nullptr || m_model->currentUser()->name().isEmpty()) {
            qCWarning(DDE_SHELL) << "Current user is invalid or current user's name is empty";
            return;
        }
        if (active) {
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            endAuthentication(m_account, AT_All);
            destroyAuthentication(m_account);
        }
    });

    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [this](bool active) {
        const bool sessionIsActive = m_login1SessionSelf->active();
        qDebug() << "Prepare for sleep, whether set dpms: " << (!active ? "on" : "off")
                 << ", curren session is active:" << sessionIsActive;
        // session 未激活的时候使用 dde-wldpms 无效，且 dde-wldpms 无法收到 modeChanged 信号，会一直处于等待状态不退出
        if (sessionIsActive)
            screenSwitchByWldpms(!active);
    });

    /* org.freedesktop.login1.Manager */
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [this](bool isSleep) {
        qCInfo(DDE_SHELL) << "DBus login1 manager prepare for sleep:" << isSleep;
        // 登录界面待机或休眠时提供显示假黑屏，唤醒时显示正常界面
        m_model->setIsBlackMode(isSleep);

        if (isSleep) {
            endAuthentication(m_account, AT_All);
            destroyAuthentication(m_account);
        } else {
            if (!m_model->currentUser()->isNoPasswordLogin()) {
                bool useOnekeylogin = DConfigHelper::instance()->getConfig("enableOneKeylogin", false).toBool();
                if (useOnekeylogin) {
                    // 当有一键登录的时候，需要保证一键登录插件中待机唤醒处理逻辑在此之前；否则greeter待机唤醒一键登录功能异常
                    QTimer::singleShot(0, this, [this]{
                        createAuthentication(m_model->currentUser()->name());
                    });
                } else {
                    createAuthentication(m_model->currentUser()->name());
                }
            }
        }
    });
    connect(m_login1Inter, &DBusLogin1Manager::SessionRemoved, this, [this] {
        qCInfo(DDE_SHELL) << "DBus login1 manager session removed";
        if (m_model->updateCurrentUser(m_lockInter->CurrentUser())) {
            m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
            destroyAuthentication(m_account);
            if (!m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            } else {
                m_model->setAuthType(AT_None);
            }
        }
#ifndef ENABLE_DSS_SNIPE
        m_soundPlayerInter->PrepareShutdownSound(static_cast<int>(m_model->currentUser()->uid()));
#else
        prepareShutdownSound();
#endif
    });
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [this](const QString &json) {
        qCInfo(DDE_SHELL) << "User changed: " << json;
        // 如果是已登录用户则返回，否则已登录用户和未登录用户来回切换时会造成用户信息错误
        std::shared_ptr<User> user_ptr = m_model->json2User(json);
        if (!user_ptr || user_ptr->isLogin())
            return;

        if (user_ptr->name() == User().name()) {
            qCWarning(DDE_SHELL) << "Avoid set invalid user from lockservice";
            return;
        }

        m_resetSessionTimer->stop();
        m_model->updateCurrentUser(user_ptr);
        const QString &account = user_ptr->name();
        if (user_ptr.get()->isNoPasswordLogin()) {
            emit m_model->authTypeChanged(AT_None);
            m_account = account;
        }
        QTimer::singleShot(100, this, [this] {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        });
    });
    /* model */
    connect(m_model, &SessionBaseModel::authTypeChanged, this, [this](const AuthFlags type) {
        qCInfo(DDE_SHELL) << "Auth type changed, incoming type:" << type
                << ", mfa flag:" << m_model->getAuthProperty().MFAFlag;
        if (type > 0 && m_model->getAuthProperty().MFAFlag) {
            startAuthentication(m_account, m_model->getAuthProperty().AuthType);
        }
        m_limitsUpdateTimer->start();
    });
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, &GreeterWorker::doPowerAction);
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &GreeterWorker::onCurrentUserChanged);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [this](bool visible) {
        if (visible) {
            if (m_authFramework->isDAStartupCompleted() && !m_model->isServerModel()
                && (!m_model->currentUser()->isNoPasswordLogin() || m_model->currentUser()->expiredState() == User::ExpiredAlready)) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            m_resetSessionTimer->stop();
        }
    });
    /* 数字键盘状态 */
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::numLockStatusChanged, this, [this](bool on) {
        saveNumLockState(m_model->currentUser(), on);
    });

    connect(KeyboardMonitor::instance(), &KeyboardMonitor::initialized, this, [this] {
        if (m_model) {
            recoveryUserKBState(m_model->currentUser());
        }
    });

    connect(m_limitsUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_authFramework->isDeepinAuthValid())
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_account));
    });

    // 等保服务开启/关闭时更新
    QDBusConnection::systemBus().connect(SECURITY_ENHANCE_NAME, SECURITY_ENHANCE_PATH, SECURITY_ENHANCE_NAME,
                                         "Receipt", this, SLOT(onReceiptChanged(bool)));

    connect(m_authFramework, &DeepinAuthFramework::startupCompleted, this, [this] {
        /* com.deepin.daemon.Authenticate */
        m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
        m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
        m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));

        if (m_model->visible() && !m_model->isServerModel()
            && (!m_model->currentUser()->isNoPasswordLogin() || m_model->currentUser()->expiredState() == User::ExpiredAlready)) {
            createAuthentication(m_model->currentUser()->name());
        }
    });

    // 监听terminal锁定状态
    QDBusConnection::systemBus().connect(DSS_DBUS::accountsService, DSS_DBUS::accountsPath, "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged", this, SLOT(terminalLockedChanged(QDBusMessage)));

    connect(&SignalBridge::ref(), &SignalBridge::requestSendExtraInfo, this, &GreeterWorker::sendExtraInfo);
}

void GreeterWorker::initData()
{
    if (isSecurityEnhanceOpen()) {
        qCInfo(DDE_SHELL) << "Security enhance is open";
        m_model->setSEType(true);
    }

    auto list = m_accountsInter->userList();
    if (!list.isEmpty()) {
        /* com.deepin.daemon.Accounts */
        m_model->updateUserList(list);
        m_model->updateLoginedUserList(m_loginedInter->userList());

        m_model->setUserlistVisible(valueByQSettings<bool>("", "userlist", true));
        /* com.deepin.udcp.iam */
        QDBusInterface ifc(DSS_DBUS::udcpIamService, DSS_DBUS::udcpIamPath, DSS_DBUS::udcpIamService, QDBusConnection::systemBus(), this);
        const bool allowShowCustomUser = (!m_model->userlistVisible()) || valueByQSettings<bool>("", "loginPromptInput", false) ||
            ifc.property("Enable").toBool() || checkIsADDomain();
        m_model->setAllowShowCustomUser(allowShowCustomUser);

        /* init current user */
        if (DSysInfo::deepinType() == DSysInfo::DeepinServer || m_model->allowShowCustomUser()) {
            // 如果是服务器版本或者loginPromptInput配置为true，默认显示空用户
            std::shared_ptr<User> user(new User());
            m_model->setIsServerModel(DSysInfo::deepinType() == DSysInfo::DeepinServer);
            m_model->addUser(user);
            if (DSysInfo::deepinType() == DSysInfo::DeepinServer || valueByQSettings<bool>("", "loginPromptInput", false) || !m_model->userlistVisible()) {
                m_model->updateCurrentUser(user);
            } else {
                /* com.deepin.dde.LockService */
                m_model->updateCurrentUser(m_lockInter->CurrentUser());
            }
        } else {
            /* com.deepin.dde.LockService */
            m_model->updateCurrentUser(m_lockInter->CurrentUser());
        }
#ifndef ENABLE_DSS_SNIPE
    m_soundPlayerInter->PrepareShutdownSound(static_cast<int>(m_model->currentUser()->uid()));
#else
    prepareShutdownSound();
#endif
    } else {
        qCWarning(DDE_SHELL) << "dbus com.deepin.daemon.Accounts userList is empty, use ...";
        m_model->setAllowShowCustomUser(true);
        std::shared_ptr<User> user(new User());
        m_model->addUser(user);
        m_model->updateCurrentUser(user);
    }

    /* com.deepin.daemon.Authenticate */
    if (m_authFramework->isDAStartupCompleted() ) {
        m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
        m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
        m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
    }

    // 获取terminal锁定状态
    QDBusInterface accoutsInter(DSS_DBUS::accountsService, DSS_DBUS::accountsPath, DSS_DBUS::accountsService, QDBusConnection::systemBus(), this);
    m_model->setTerminalLocked(accoutsInter.property("IsTerminalLocked").toBool());
}

void GreeterWorker::initConfiguration()
{
#ifndef ENABLE_DSS_SNIPE
    m_model->setAlwaysShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Ondemand);
#else
    m_model->setAlwaysShowUserSwitchButton(getDconfigValue("switchUser", Ondemand).toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getDconfigValue("switchUser", Ondemand).toInt() == AuthInterface::Ondemand);
#endif

    checkPowerInfo();

    if (QFile::exists("/etc/deepin/no_suspend")) {
        m_model->setCanSleep(false);
    }

    // 当这个配置不存在是，如果是不是笔记本就打开小键盘，否则就关闭小键盘 0关闭键盘 1打开键盘 2默认值（用来判断是不是有这个key）
    if (m_model->currentUser() != nullptr && getNumLockState(m_model->currentUser()->name()) == NUM_LOCK_UNKNOWN) {
        PowerInter powerInter(DSS_DBUS::powerService, DSS_DBUS::powerPath, QDBusConnection::systemBus(), this);
        if (powerInter.hasBattery()) {
            saveNumLockState(m_model->currentUser(), false);
        } else {
            saveNumLockState(m_model->currentUser(), true);
        }
    }
    recoveryUserKBState(m_model->currentUser());
}

void GreeterWorker::doPowerAction(const SessionBaseModel::PowerAction action)
{
    switch (action) {
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        break;
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        break;
    // 在登录界面请求待机或者休眠时，通过显示假黑屏挡住输入密码界面，防止其闪现
    case SessionBaseModel::PowerAction::RequireSuspend:
        m_model->setIsBlackMode(true);
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        m_login1Inter->Suspend(true);
        break;
    case SessionBaseModel::PowerAction::RequireHibernate:
        m_model->setIsBlackMode(true);
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
void GreeterWorker::setCurrentUser(const std::shared_ptr<User> user)
{
    // 不要把用于域用户UI控制的“...”用户写给后端
    if (user && user->name() == User().name()) {
        qCWarning(DDE_SHELL) << "Avoid set invalid user to lockservice";
        return;
    }

    qCInfo(DDE_SHELL) << "Set current user, user:" << user->name();
    QJsonObject json;
    json["AuthType"] = static_cast<int>(user->lastAuthType());
    json["Name"] = user->name();
    json["Type"] = user->type();
    json["Uid"] = static_cast<int>(user->uid());
    json["LastCustomAuth"] = user->lastCustomAuth(); // 上一次认证通过自定义认证类型
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();
}

void GreeterWorker::switchToUser(std::shared_ptr<User> user)
{
    if (*user == *m_model->currentUser()) {
        if (!m_model->currentUser()->isNoPasswordLogin()) {
            createAuthentication(user->name());
        }
        return;
    }
    qCInfo(DDE_SHELL) << "Switch user from" << m_account
            << ", to:" << user->name()
            << ", user id:" << user->uid()
            << ", is login:" << user->isLogin();
    endAuthentication(m_account, AT_All);

    if (user->uid() == INT_MAX) {
        m_model->setAuthType(AT_None);
    }

    setCurrentUser(user);
    if (user->isLogin()) { // switch to user Xorg
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
        destroyAuthentication(m_account);
        m_model->updateCurrentUser(user);
        if (!user->isNoPasswordLogin()) {
            createAuthentication(user->name());
        } else {
            m_model->setAuthType(AT_None);
        }
#ifndef ENABLE_DSS_SNIPE
        m_soundPlayerInter->PrepareShutdownSound(static_cast<int>(m_model->currentUser()->uid()));
#else
        prepareShutdownSound();
#endif
    }
}

bool GreeterWorker::isSecurityEnhanceOpen()
{
    QDBusInterface securityEnhanceInterface(SECURITY_ENHANCE_NAME,
                               SECURITY_ENHANCE_PATH,
                               SECURITY_ENHANCE_NAME,
                               QDBusConnection::systemBus());
    QDBusReply<QString> reply = securityEnhanceInterface.call("Status");
    if (!reply.isValid()) {
       qCWarning(DDE_SHELL) << "Get security enhance status error: " << reply.error();
       return false;
    }
    return reply.value() == "open" || reply.value() == "opening";
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void GreeterWorker::createAuthentication(const QString &account)
{
    qCInfo(DDE_SHELL) << "Create auth entication, account: " << account;
    if (m_model->terminalLocked()) {
        // 切换用户后，需要触发TerminalLocked信号
        m_model->sendTerminalLockedSignal();
        return;
    }

    m_account = account;
    if (account.isEmpty() || account == "...") {
        m_model->setAuthType(AT_None);
        return;
    }

    // 如果验证会话已经存在，销毁后重新开启验证
    // TODO 这个处理办法稳健但是比较粗暴，后面需要结合DA梳理一下认证session创建、销毁和复用的时机，优化流程
    if (m_authFramework->authSessionExist(account)) {
        endAuthentication(account, AT_All);
        destroyAuthentication(account);
    }

    m_retryAuth = false;
    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    if (user_ptr) {
        user_ptr->updatePasswordExpiredInfo();
    } else {
        // 域管账户第一次登录时，后端还未提供账户信息，获取不到用户密码过期数据
        // 需要通过glibc接口读取
        updatePasswordExpiredStateBySPName(account);
    }

    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), Login);
        m_authFramework->SetAuthQuitFlag(account, DeepinAuthFramework::ManualQuit);
        startGreeterAuth(account);
        break;
    default:
        startGreeterAuth(account);
        m_model->setAuthType(AT_PAM);
        break;
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void GreeterWorker::destroyAuthentication(const QString &account)
{
    qCInfo(DDE_SHELL) << "Destroy authentication, account:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->DestroyAuthController(account);
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
void GreeterWorker::startAuthentication(const QString &account, const AuthFlags authType)
{
    if (!m_model->currentUser()->allowToChangePassword()) {
        qCInfo(DDE_SHELL) << "Authentication exit because of user's authority";
        return;
    }
    qCInfo(DDE_SHELL) << "Start authentication, account:" << account << ", auth type:" << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->StartAuthentication(account, authType, -1);
        startGreeterAuth(account);
        break;
    default:
        startGreeterAuth(account);
        break;
    }
}

/**
 * @brief 将密文发送给认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void GreeterWorker::sendTokenToAuth(const QString &account, const AuthType authType, const QString &token)
{
    qCInfo(DDE_SHELL) << "Send token:" << account << ", auth type:" << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (AT_PAM == authType) {
            m_greeter->respond(token);
        } else {
            m_authFramework->SendTokenToAuth(account, authType, token);
        }
        if (authType == AT_Password) {
            m_password = token; // 用于解锁密钥环
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
void GreeterWorker::endAuthentication(const QString &account, const AuthFlags authType)
{
    qCInfo(DDE_SHELL) << "End authentication, account:" << account << ", auth type:" << authType;

    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (authType == AT_All)
            m_authFramework->SetPrivilegesDisable(account);

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
void GreeterWorker::checkAccount(const QString &account, bool switchUser)
{
    qCInfo(DDE_SHELL) << "Check account, account: " << account;
    const auto &originalUsePtr = m_model->findUserByName(account);
    std::shared_ptr<User> user_ptr = originalUsePtr;
    // 当用户登录成功后，判断用户输入帐户有效性逻辑改为后端去做处理
    const QString userPath = m_accountsInter->FindUserByName(account);
    if (userPath.startsWith("/")) {
        user_ptr = std::make_shared<NativeUser>(userPath);
        // 对于没有设置密码的账户,直接认定为错误账户
        if (!user_ptr->isPasswordValid()) {
            qCWarning(DDE_SHELL) << "The user's password is invalid, user path: " << userPath;
            emit m_model->accountError();
            emit m_model->authFailedTipsMessage(tr("Wrong account"));
            m_model->setAuthType(AT_None);
            return;
        }
    } else if (!user_ptr) {
        // 判断账户第一次登录时的有效性
        std::string str = account.toStdString();
        passwd *pw = getpwnam(str.c_str());
        if (pw) {
            QString userName = pw->pw_name;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QString userFullName = userName.left(userName.indexOf(QString("@")));
#else
            QString userFullName = userName.leftRef(userName.indexOf(QString("@"))).toString();
#endif
            user_ptr = std::make_shared<ADDomainUser>(INT_MAX - 1);
            dynamic_cast<ADDomainUser *>(user_ptr.get())->setName(userName);
            dynamic_cast<ADDomainUser *>(user_ptr.get())->setFullName(userFullName);
        } else {
            qCWarning(DDE_SHELL) << "Password is null, user path: " << userPath;
            emit m_model->accountError();
            emit m_model->authFailedTipsMessage(tr("Wrong account"));
            m_model->setAuthType(AT_None);
            return;
        }
    }

    // m_greeter->cancelAuthentication后m_greeter的数据不会发生改变，当前账户如果是无密码登陆则不需要判断m_greeter认证数据
    if (m_greeter->authenticationUser() == account && m_account == account && !user_ptr->isNoPasswordLogin()) {
        qCInfo(DDE_SHELL) << "The current user is the incoming user, do not check again";
        return;
    }

    // 如果用户已经登录了，直接切换用户
    if (switchUser && originalUsePtr && originalUsePtr->isLogin() && user_ptr) {
        qCInfo(DDE_SHELL) << "Current user is already logged in";
        Q_EMIT m_model->checkAccountFinished();
        user_ptr->updateLoginState(true); // 初始化 NativeUser 的时候没有处理登录状态
        switchToUser(user_ptr);
        return;
    }

    m_model->updateCurrentUser(user_ptr);
    if (user_ptr->isNoPasswordLogin()) {
        if (user_ptr->expiredState() == User::ExpiredAlready) {
            changePasswd();
        }
        startGreeterAuth(user_ptr->name());
    } else {
        m_resetSessionTimer->stop();
        endAuthentication(m_account, AT_All);
        m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
        destroyAuthentication(m_account);
        createAuthentication(user_ptr->name());
    }
}

void GreeterWorker::checkSameNameAccount(const QString &account, bool switchUser)
{
    qCInfo(DDE_SHELL) << "Start check same name account, name: " << account;
    QDBusInterface ifc(DSS_DBUS::udcpIamService, DSS_DBUS::udcpIamPath, DSS_DBUS::udcpIamService, QDBusConnection::systemBus());
    QDBusPendingReply<QString> reply = ifc.asyncCall(QStringLiteral("GetPwName"), account);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, account, switchUser, reply, watcher] {
        if (reply.error().message().isEmpty()) {
            const auto &originalUsePtr = m_model->findUserByName(account);
            const QString &userDetail = reply.value();
            if (originalUsePtr && !originalUsePtr->isDomainUser() && !userDetail.isEmpty()) {
                qCInfo(DDE_SHELL) << "Find same name account, user detail: " << userDetail;
                Q_EMIT requestShowUsersWithTheSameName(account, userDetail);
            } else {
                qCInfo(DDE_SHELL) << "Not find same name account";
                checkAccount(account, switchUser);
            }
        } else {
            qCWarning(DDE_SHELL) << "Check same name account failed, error:" << reply.error().message();
            checkAccount(account, switchUser);
        }
        watcher->deleteLater();
    });
}

void GreeterWorker::checkDBusServer(bool isValid)
{
    if (isValid) {
        m_accountsInter->userList();
    } else {
        // FIXME: 我不希望这样做，但是QThread::msleep会导致无限递归
        QTimer::singleShot(300, this, [this] {
            qCWarning(DDE_SHELL) << "com.deepin.daemon.Accounts is not start, rechecking!";
            checkDBusServer(m_accountsInter->isValid());
        });
    }
}

/**
 * @brief 显示提示信息
 *
 * @param text
 * @param type
 */
void GreeterWorker::showPrompt(const QString &text, const QLightDM::Greeter::PromptType type)
{
    qCInfo(DDE_SHELL) << "Greeter prompt: " << text << ", type:" << type;
    m_model->setLightdmPamStarted(true);
    switch (type) {
    case QLightDM::Greeter::PromptTypeSecret:
        m_retryAuth = true;
        m_model->updateAuthState(AT_PAM, AS_Prompt, text);
        break;
    case QLightDM::Greeter::PromptTypeQuestion:
        m_model->updateAuthState(AT_PAM, AS_Prompt, text);
        break;
    }
}

/**
 * @brief 显示认证成功/失败的信息
 *
 * @param text
 * @param type
 */
void GreeterWorker::showMessage(const QString &text, const QLightDM::Greeter::MessageType type)
{
    qCInfo(DDE_SHELL) << "Greeter message:" << text << ", type:" << type;
    m_model->setLightdmPamStarted(true);
    switch (type) {
    case QLightDM::Greeter::MessageTypeInfo:
        m_model->updateAuthState(AT_PAM, AS_Prompt, text);
        break;
    case QLightDM::Greeter::MessageTypeError:
        m_retryAuth = false;
        m_model->updateAuthState(AT_PAM, AS_Failure, text);
        break;
    }
}

/**
 * @brief 认证完成
 */
void GreeterWorker::authenticationComplete()
{
    const bool result = m_greeter->isAuthenticated();
    qCInfo(DDE_SHELL) << "Authentication result:" << result << ", retry auth" << m_retryAuth;

    if (!result) {
        if (m_retryAuth && !m_model->getAuthProperty().MFAFlag) {
            showMessage(tr("Wrong Password"), QLightDM::Greeter::MessageTypeError);
        }
        if (!m_model->currentUser()->limitsInfo(AT_Password).locked) {
            m_greeter->authenticate(m_model->currentUser()->name());
        }
        return;
    }

    emit m_model->authFinished(true);

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

#ifndef ENABLE_DSS_SNIPE
    DDE_EventLogger::EventLoggerData data;
    data.tid = EVENT_LOGGER_GREETER_LOGIN;
    data.event = "startup";
    data.target = "dde-session-shell";
    data.message = {{"startup", "authenticationComplete"}, {"result", QString::number(result)}};
    DDE_EventLogger::EventLogger::instance().writeEventLog(data);
#endif

    qCInfo(DDE_SHELL) << "Start session: " << m_model->sessionKey();

    emit requestUpdateBackground(m_model->currentUser()->greeterBackground());
    setCurrentUser(m_model->currentUser());
    m_greeter->startSessionSync(m_model->sessionKey());
    endAuthentication(m_account, AT_All);
    destroyAuthentication(m_account);

    //清理所有TTY输出
    QDBusReply<void> reply = m_systemDaemon->call("ClearTtys");
    if (!reply.isValid()) {
        qCWarning(DDE_SHELL) << "Clear ttys failed, error messages: " << reply.error().message();
    };
}

void GreeterWorker::onAuthFinished()
{
    qCInfo(DDE_SHELL) << "Auth finished";
    if (m_greeter->inAuthentication()) {
        m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
        updatePasswordExpiredStateBySPName(m_account);
        if (m_model->currentUser()->expiredState() == User::ExpiredAlready) {
            changePasswd();
        }
    } else {
        qCWarning(DDE_SHELL) << "The lightdm is not in authentication!";
    }
}

void GreeterWorker::onAuthStateChanged(const int type, const int state, const QString &message)
{
    qCInfo(DDE_SHELL) << "Auth type:" << type << ", state:" << state << ", message:" << message;
    if (m_model->getAuthProperty().MFAFlag) {
        if (type == AT_All) {
            switch (state) {
            case AS_Success:
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                m_resetSessionTimer->stop();
                if (m_greeter->inAuthentication()) {
                    m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
                    if (m_model->currentUser()->expiredState() == User::ExpiredAlready) {
                        changePasswd();
                    }
                } else {
                    qCWarning(DDE_SHELL) << "The lightdm is not in authentication!";
                }
                break;
            case AS_Cancel:
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                destroyAuthentication(m_account);
                break;
            default:
                break;
            }
        } else {
            switch (state) {
            case AS_Success:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

                // 处于序列化多因认证过程中时不要重置认证
                if (MFASequenceControl::instance().currentAuthType() == AuthType::AT_None) {
                    m_resetSessionTimer->start();
                }

                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                break;
            case AS_Failure:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
                endAuthentication(m_account, AuthType(type));
                // 人脸和虹膜需要手动重启验证
                if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                    QTimer::singleShot(50, this, [this, type] {
                        startAuthentication(m_account, AuthType(type));
                    });
                }
                QTimer::singleShot(50, this, [=] {
                    m_model->updateAuthState(AuthType(type), AuthState(state), message);
                });
                break;
            case AS_Locked:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                endAuthentication(m_account, AuthType(type));
                // TODO: 信号时序问题,考虑优化,Bug 89056
                QTimer::singleShot(50, this, [=] {
                    m_model->updateAuthState(AuthType(type), AuthState(state), message);
                });
                break;
            case AS_Timeout:
            case AS_Error:
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                endAuthentication(m_account, AuthType(type));
                break;
            case AS_Unlocked:
                m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                break;
            default:
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                break;
            }
        }
    } else {
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
            && (state == AS_Success || state == AS_Failure)
            && m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
        m_model->updateAuthState(AuthType(type), AuthState(state), message);
        switch (state) {
        case AS_Success:
            if (m_model->currentUser()->isLdapUser() && type != AT_Custom && type != AT_All) {
                auto loginType = LPUtil::loginType();
                if (loginType == LPUtil::Type::CLT_MFA) {
                    m_resetSessionTimer->setInterval(LPUtil::sessionTimeout());
                    m_resetSessionTimer->start();
                    break;
                }
            }

            if (type == AT_Face || type == AT_Iris) {
                m_resetSessionTimer->setInterval(DEFAULT_SESSION_TIMEOUT);
                m_resetSessionTimer->start();
            }
            break;
        case AS_Failure:
            if (AT_All != type) {
                endAuthentication(m_account, AuthType(type));
                // 人脸和虹膜需要手动重新开启验证
                if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                    QTimer::singleShot(50, this, [this, type] {
                        startAuthentication(m_account, AuthType(type));
                    });
                }
            }
            break;
        case AS_Cancel:
            destroyAuthentication(m_account);
            break;
        default:
            break;
        }
    }
}

void GreeterWorker::onReceiptChanged(bool state)
{
    Q_UNUSED(state)

    m_model->setSEType(isSecurityEnhanceOpen());
}

void GreeterWorker::onCurrentUserChanged(const std::shared_ptr<User> &user)
{
    recoveryUserKBState(user);
    loadTranslation(user->locale());
    //  账户在变换的时候需要通知lockservice
    setCurrentUser(user);
    emit requestUpdateBackground(m_model->currentUser()->greeterBackground());
}

void GreeterWorker::onSessionCreated()
{
    qCInfo(DDE_SHELL) << "Set privileges enable";
    if (!m_authFramework->SetPrivilegesEnable(m_account, QString("/usr/sbin/lightdm"))) {
        qCWarning(DDE_SHELL) << "Failed to set privileges!";
    }
}

void GreeterWorker::saveNumLockState(std::shared_ptr<User> user, bool on)
{
    QStringList list = DConfigHelper::instance()->getConfig("numLockState", QStringList()).toStringList();
    const QString &userName = user->name();

    // 移除当前用户的记录
    for (const QString &numLockState : list) {
        QStringList tmpList = numLockState.split(":");
        if (tmpList.size() == 2 && tmpList.at(0) == userName) {
            list.removeAll(numLockState);
            break;
        }
    }

    // 插入当前用户的记录
    list.append(user->name() + ":" + (on ? "True" : "False"));
    DConfigHelper::instance()->setConfig("numLockState", list);
}

int GreeterWorker::getNumLockState(const QString &userName)
{
    QStringList list = DConfigHelper::instance()->getConfig("numLockState", QStringList()).toStringList();

    for (const QString &numLockState : list) {
        QStringList tmpList = numLockState.split(":");
        if (tmpList.size() == 2 && tmpList.at(0) == userName) {
            return "True" == tmpList.at(1) ? NUM_UNLOCKED : NUM_LOCKED;
        }
    }

    return NUM_LOCK_UNKNOWN;
}

void GreeterWorker::recoveryUserKBState(std::shared_ptr<User> user)
{
    if (user.get() == nullptr)
        return;

    const bool enabled = getNumLockState(user->name()) == NUM_UNLOCKED;

    qCWarning(DDE_SHELL) << "Restore numlock state to " << enabled;

    // Resync numlock light with numlock status
    if (!m_model->isUseWayland()) {
        bool currentNumLock = KeyboardMonitor::instance()->isNumLockOn();
        KeyboardMonitor::instance()->setNumLockStatus(!currentNumLock);
        KeyboardMonitor::instance()->setNumLockStatus(currentNumLock);
    }

    KeyboardMonitor::instance()->setNumLockStatus(enabled);
}

void GreeterWorker::restartResetSessionTimer()
{
    if (m_model->visible() && m_resetSessionTimer->isActive()) {
        m_resetSessionTimer->start();
    }
}

void GreeterWorker::startGreeterAuth(const QString &account)
{
    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    // m_greeter->cancelAuthentication后m_greeter的数据不会发生改变，当前账户如果是无密码登陆则不需要判断m_greeter认证数据
    if (!m_greeter->inAuthentication() || m_greeter->authenticationUser() != account || (user_ptr && user_ptr->isNoPasswordLogin())) {
        m_greeter->authenticate(account);
    } else {
        qCInfo(DDE_SHELL) << "Lightdm is in authentication, won't do it again, account: " << account;
    }
}

void GreeterWorker::changePasswd()
{
    qCInfo(DDE_SHELL) << "Change password";
    m_model->updateMFAFlag(false);
    m_model->setAuthType(AT_PAM);
}

void GreeterWorker::screenSwitchByWldpms(bool active)
{
    qDebug() << "Greeter worker screen switch by Wldpms:" << active;
    QStringList arguments;
    arguments << "-s";
    if (active) {
        arguments << "on";
    } else {
        arguments << "off";
    }
    QProcess::startDetached("dde_wldpms", arguments);
}

void GreeterWorker::updatePasswordExpiredStateBySPName(const QString &account)
{
    std::string str = account.toStdString();
    spwd *pw = getspnam(str.c_str());

    if (pw) {
        const int secondsPerDay = 60 * 60 * 24;

        long int spMax = pw->sp_max;
        long int spWarn = pw->sp_warn;
        long int spLastChg = pw->sp_lstchg;

        User::ExpiredState state = User::ExpiredNormal;
        int days = 0;

        if (spLastChg == 0) {
            // expired
            state = User::ExpiredAlready;
            days = 0;
        }

        if (spMax == -1) {
            // never expired
            state = User::ExpiredNormal;
            days = -1;
        }

        int curDays = QDateTime::currentSecsSinceEpoch() / secondsPerDay;
        int daysLeft = spLastChg + spMax - curDays;

        if (daysLeft < 0) {
            state = User::ExpiredAlready;
            days = daysLeft;
        } else if (spWarn > daysLeft) {
            state = User::ExpiredSoon;
            days = daysLeft;
        }
        if (m_model->currentUser()) {
            m_model->currentUser()->updatePasswordExpiredState(state, days);
        }
    }
}

void GreeterWorker::terminalLockedChanged(const QDBusMessage &msg)
{
    QList<QVariant> arguments = msg.arguments();
    if (arguments.size() != 3) {
        qCWarning(DDE_SHELL) << "Terminal locked changed invalid arguments size, size: " << arguments.size();
        return;
    }

    QString interfaceName = arguments.at(0).toString();
    if (interfaceName != QString("com.deepin.daemon.Accounts")){
        return;
    }

    QVariantMap changedProps = qdbus_cast<QVariantMap>(arguments.at(1).value<QDBusArgument>());
    if (changedProps.contains("IsTerminalLocked")) {
        bool locked = changedProps.value("IsTerminalLocked").toBool();
        qCInfo(DDE_SHELL) << "Terminal locked changed, locked: " << locked;
        m_model->setTerminalLocked(locked);

        if (!locked) {
            createAuthentication(m_model->currentUser()->name());
        }
    }
}

void GreeterWorker::resetAuth(const QString &account)
{
    qCWarning(DDE_SHELL) << "reset auth for user : " << account;
    if (account.isEmpty()) {
        return;
    }

    endAuthentication(account, AT_All);
    m_model->updateAuthState(AT_All, AS_Cancel, "Cancel");
    destroyAuthentication(account);
    createAuthentication(account);
}

void GreeterWorker::onNoPasswordLoginChanged(const QString &account, bool noPassword)
{
    if(m_model->currentUser()->name() == account && m_greeter->authenticationUser() == account) {

        if (noPassword && m_greeter->inAuthentication()) {
            m_greeter->cancelAuthentication();
            return;
        }

        if (!noPassword && !m_greeter->inAuthentication()) {
            createAuthentication(account);
        }
    }
}

#ifdef ENABLE_DSS_SNIPE
void GreeterWorker::prepareShutdownSound()
{
    QDBusInterface soundPlayerInter("org.deepin.dde.SoundThemePlayer1", "/org/deepin/dde/SoundThemePlayer1",
        "org.deepin.dde.SoundThemePlayer1", QDBusConnection::systemBus());
    if (!soundPlayerInter.isValid() || !m_model->currentUser()) {
        qCWarning(DDE_SHELL) << "Sound player interface is not valid or current user is null:" << soundPlayerInter.isValid();
        return;
    }

    //soundPlayerInter.call("PrepareShutdownSound", static_cast<int>(m_model->currentUser()->uid()));
}
#endif

void GreeterWorker::sendExtraInfo(const QString &account, AuthCommon::AuthType authType, const QString &info)
{
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->sendExtraInfo(account, authType, info);
        break;
    default:
        break;
    }
}
