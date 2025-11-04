// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockworker.h"

#include "authcommon.h"
#include "sessionbasemodel.h"
#include "userinfo.h"
#include "login_plugin_util.h"
#include "public_func.h"
#include "dconfig_helper.h"
#include "warningcontent.h"
#include "fullscreenbackground.h"
#include "updateworker.h"
#include "lockcontent.h"
#include "signal_bridge.h"
#include "mfasequencecontrol.h"

#include <DSysInfo>

#include <QProcess>

#include <libintl.h>
#include <unistd.h>

#include "dbusconstant.h"

#define DOMAIN_BASE_UID 10000

using namespace Auth;
using namespace AuthCommon;
DCORE_USE_NAMESPACE

LockWorker::LockWorker(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_authenticating(false)
    , m_isThumbAuth(false)
    , m_authFramework(new DeepinAuthFramework(this))
    , m_lockInter(new DBusLockService(DSS_DBUS::lockService, DSS_DBUS::lockServicePath, QDBusConnection::systemBus(), this))
    , m_hotZoneInter(new DBusHotzone(DSS_DBUS::zoneService, DSS_DBUS::zonePath, QDBusConnection::sessionBus(), this))
    , m_resetSessionTimer(new QTimer(this))
    , m_limitsUpdateTimer(new QTimer(this))
    , m_sessionManagerInter(new SessionManagerInter(DSS_DBUS::sessionManagerService, DSS_DBUS::sessionManagerPath, QDBusConnection::sessionBus(), this))
    , m_switchosInterface(new HuaWeiSwitchOSInterface("com.huawei", "/com/huawei/switchos", QDBusConnection::sessionBus(), this))
    , m_kglobalaccelInter(nullptr)
    , m_kwinInter(nullptr)
{
    initConnections();
    initData();
    initConfiguration();

    m_limitsUpdateTimer->setSingleShot(true);
    m_limitsUpdateTimer->setInterval(50);

    m_resetSessionTimer->setInterval(15000);

#ifndef ENABLE_DSS_SNIPE
    if (m_gsettings != nullptr && m_gsettings->keys().contains("authResetTime")){
        int resetTime = m_gsettings->get("auth-reset-time").toInt();
        if(resetTime > 0)
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
        endAuthentication(m_account, AT_All);
        destroyAuthentication(m_account);
        createAuthentication(m_account);
    });
}

/**
 * @brief 初始化信号连接
 */
void LockWorker::initConnections()
{
    /* com.deepin.daemon.Accounts */
    connect(m_accountsInter, &AccountsInter::UserAdded, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::addUser));
    connect(m_accountsInter, &AccountsInter::UserDeleted, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::removeUser));
    connect(m_accountsInter, &AccountsInter::UserDeleted, this, [this](const QString &path) {
        if (path == m_model->currentUser()->path()) {
            m_model->updateCurrentUser(m_lockInter->CurrentUser());
        }
    });
    connect(m_loginedInter, &LoginedInter::UserListChanged, m_model, &SessionBaseModel::updateLoginedUserList);
    /* com.deepin.daemon.Authenticate */
    connect(m_authFramework, &DeepinAuthFramework::FramworkStateChanged, m_model, &SessionBaseModel::updateFrameworkState);
    connect(m_authFramework, &DeepinAuthFramework::LimitsInfoChanged, this, [this](const QString &account) {
        if (account == m_model->currentUser()->name()) {
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::DeviceChanged, this, [this](const int type, const int state) {
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
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    connect(m_authFramework, &DeepinAuthFramework::AuthStateChanged, this, &LockWorker::onAuthStateChanged);
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [this](const QString &json) {
        qCInfo(DDE_SHELL) << "DBus lock service user changed, user: " << json;
        QTimer::singleShot(100, this, [this] {
            if(m_model->currentContentType() == SessionBaseModel::UpdateContent){
                qCInfo(DDE_SHELL) << "Is updating, do not answer user changed signal";
                return;
            }

            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        });
        m_resetSessionTimer->stop();
    });
    connect(m_lockInter, &DBusLockService::Event, this, &LockWorker::lockServiceEvent);
    /* com.deepin.SessionManager */
    connect(m_sessionManagerInter, &SessionManagerInter::Unlock, this, [this] {
        m_authenticating = false;
        m_password.clear();
        emit m_model->authFinished(true);
    });

#ifdef ENABLE_DSS_SNIPE
    connect(m_sessionManagerInter, &SessionManagerInter::LockedChanged, this, [ this ](bool locked) {
        qDebug() << "SessionManagerInter::LockedChanged" << locked;
        if (locked && !m_model->visible()) {
            m_model->showLockScreen();
        }
    });
#endif

    /* org.freedesktop.login1.Session */
    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [this](bool active) {
        qCInfo(DDE_SHELL) << "DBus lock service active changed, active: " << active << ", model visible: " << m_model->visible();
        if (m_model->currentContentType() == SessionBaseModel::UpdateContent) {
            qCInfo(DDE_SHELL) << "Updating, do not answer login1 session self `ActiveChanged` signal";
            return;
        }

        if (active && m_model->visible()) {
            createAuthentication(m_model->currentUser()->name());
        } else {
            endAuthentication(m_account, AT_All);
            destroyAuthentication(m_account);
        }
    });

    // 在待机时启动定时器，如果定时器超时的时间离待机时间大于15秒，则认为已经唤醒，停止定时器，并设置黑屏模式为false
    // prepareForSleep信号有时候会会延迟，所以需要定时器来确认是否已经唤醒，尽早显示出锁屏。
    static QDateTime doSuspendTime = QDateTime::currentDateTime();
    auto checkSystemWakeupTimer = new QTimer(this);
    checkSystemWakeupTimer->setInterval(500);
    connect(checkSystemWakeupTimer, &QTimer::timeout, this, [this, checkSystemWakeupTimer] {
        const int secs = static_cast<const int>(doSuspendTime.secsTo(QDateTime::currentDateTime()));
        if (secs >= 15) { // 小风险：如果15秒还没有待机下去，那就把锁屏显示出来了，不过15秒没有待机的才是大问题
            qCInfo(DDE_SHELL) << "System wakeup，hide black widget";
            checkSystemWakeupTimer->stop();
            m_model->setIsBlackMode(false);
        }
    });
    /* org.freedesktop.login1.Manager */
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [this, checkSystemWakeupTimer](bool isSleep) {
        qCInfo(DDE_SHELL) << "DBus login1 manager prepare for sleep: " << isSleep;
        if (m_model->currentContentType() == SessionBaseModel::UpdateContent) {
            qCInfo(DDE_SHELL) << "Updating, do not answer `PrepareForSleep` signal";
            return;
        }

#ifndef ENABLE_DSS_SNIPE
        const bool sleepLock = m_model->getPowerGSettings("", "sleepLock").toBool();
#else
        const bool sleepLock = isSleepLock();
#endif
        qCInfo(DDE_SHELL) << "Lock screen when system wakes up: " << sleepLock << ", is visible:" << m_model->visible();

        FullScreenBackground::setContent(LockContent::instance());
        m_model->setCurrentContentType(SessionBaseModel::LockContent);

        if (isSleep) {
            checkSystemWakeupTimer->start();
            doSuspendTime = QDateTime::currentDateTime();
            m_model->setIsBlackMode(true);
            // 休眠时按需拉起锁屏,注意此时是被动响应休眠
            if (sleepLock && m_login1SessionSelf->active()) {
                // 仅sleepLock配置开启时执行
                m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                setLocked(true);
                m_model->setVisible(true);

                qCInfo(DDE_SHELL) << "locked for sleepLock config";
            }
        } else {
            SessionBaseModel::ModeStatus status = m_model->currentModeState();
            if (isLocked() || (m_model->isNoPasswordLogin() && (status == SessionBaseModel::ModeStatus::PowerMode || status == SessionBaseModel::ModeStatus::PasswordMode))) {
                // 唤醒时如果处于lock状态，重置认证
                if (m_login1SessionSelf->active()) {
                    endAuthentication(m_account, AT_All);
                    destroyAuthentication(m_account);
                    createAuthentication(m_model->currentUser()->name());
                }
            } else {
                // 处理其他流程设置的可见状态
                m_model->setVisible(false);
            }
            // 避免在电源选项处理中设置密码模式，会导致唤醒后闪锁屏
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
            m_model->setIsBlackMode(false);
        }
        emit m_model->prepareForSleep(isSleep);
    });
    /* model */
    connect(m_model, &SessionBaseModel::authTypeChanged, this, [this](const AuthFlags type) {
        qCInfo(DDE_SHELL) << "Auth type changed: " << type;
        if (type > 0 && m_model->getAuthProperty().MFAFlag) {
            startAuthentication(m_account, type);
        }
        m_limitsUpdateTimer->start();
    });
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, &LockWorker::doPowerAction);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [this](bool visible) {
        if (visible) {
            if (m_model->currentModeState() != SessionBaseModel::ShutDownMode && m_model->currentModeState() != SessionBaseModel::UserMode) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            m_resetSessionTimer->stop();
            endAuthentication(m_account, AT_All);
            destroyAuthentication(m_model->currentUser()->name());
            setCurrentUser(m_model->currentUser());
            setLocked(false);
        }
    });
    connect(m_model, &SessionBaseModel::onStatusChanged, this, [this](SessionBaseModel::ModeStatus state) {
        if (state == SessionBaseModel::ModeStatus::PowerMode || state == SessionBaseModel::ModeStatus::ShutDownMode) {
            checkPowerInfo();
        }
    });
    /* others */
    connect(m_limitsUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_authFramework->isDeepinAuthValid())
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_account));
    });
    connect(m_dbusInter, &DBusObjectInter::NameOwnerChanged, this, [this](const QString &name, const QString &oldOwner, const QString &newOwner) {
        //该dbus监听主要处理DA升级使用，如果oldOwner为空，说明是第一次使用，不需要重新开启认证
        if (oldOwner == "")
            return;

        if (name == DSS_DBUS::authenticateService && newOwner != "" && m_model->visible() && m_sessionManagerInter->locked()) {
            m_resetSessionTimer->stop();
            endAuthentication(m_account, AT_All);
            createAuthentication(m_model->currentUser()->name());
        }
    });

    connect(m_model, &SessionBaseModel::activeAuthChanged, this, [this](const bool active) {
        const auto currentMode = m_model->currentModeState();
        const auto currenContent = m_model->currentContentType();
        qCInfo(DDE_SHELL) << "Active auth changed, active: " << active
                << ", current mode: " << currentMode
                << ", currenContent: " << currenContent;

        if (!active || currentMode == SessionBaseModel::PasswordMode || currenContent == SessionBaseModel::UpdateContent) {
            qCInfo(DDE_SHELL) << "Do not response active auth changed signal";
            return;
        }

        createAuthentication(m_model->currentUser()->name());
        m_model->setCurrentModeState(SessionBaseModel::PasswordMode);
    });

    connect(UpdateWorker::instance(), &UpdateWorker::requestExitUpdating, this, [this] {
        m_model->setVisible(false);
        m_model->setUpdatePowerMode(SessionBaseModel::UPM_None);
        m_model->setCurrentModeState(SessionBaseModel::PasswordMode);
        FullScreenBackground::setContent(LockContent::instance());
        m_model->setCurrentContentType(SessionBaseModel::LockContent);
    });
    connect(&SignalBridge::ref(), &SignalBridge::requestSendExtraInfo, this, &LockWorker::sendExtraInfo);
}

void LockWorker::initData()
{
    auto list = m_accountsInter->userList();
    if (!list.isEmpty()) {
        /* com.deepin.daemon.Accounts */
        m_model->updateUserList(m_accountsInter->userList());
        m_model->updateLoginedUserList(m_loginedInter->userList());

        m_model->setUserlistVisible(valueByQSettings<bool>("", "userlist", true));
        /* com.deepin.udcp.iam */
        QDBusInterface ifc(DSS_DBUS::udcpIamService, DSS_DBUS::udcpIamPath, DSS_DBUS::udcpIamService, QDBusConnection::systemBus(), this);
        const bool allowShowCustomUser = (!m_model->userlistVisible()) || valueByQSettings<bool>("", "loginPromptInput", false) ||
            ifc.property("Enable").toBool() || checkIsADDomain();
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
            const QString &userJson = m_lockInter->CurrentUser();
            QJsonParseError jsonParseError;
            const QJsonDocument userDoc = QJsonDocument::fromJson(userJson.toUtf8(), &jsonParseError);
            if (jsonParseError.error != QJsonParseError::NoError || userDoc.isEmpty()) {
                qCWarning(DDE_SHELL) << "Failed to obtain current user information from lock service!";
            } else {
                const QJsonObject userObj = userDoc.object();
                m_model->currentUser()->setLastAuthType(AUTH_TYPE_CAST(userObj["AuthType"].toInt()));
                m_model->currentUser()->setLastCustomAuth(userObj["LastCustomAuth"].toString());
            }
        } else {
            m_model->updateCurrentUser(m_lockInter->CurrentUser());
        }
    } else {
        qCWarning(DDE_SHELL) << "dbus com.deepin.daemon.Accounts userList is empty, use ...";

        std::shared_ptr<User> user(new User());
        m_model->addUser(user);
        m_model->updateCurrentUser(user);
        m_model->setAllowShowCustomUser(true);
    }

    /* com.deepin.daemon.Authenticate */
    m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
    m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
    m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
    m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
    if (m_model->isUseWayland()) {
        m_kglobalaccelInter = new QDBusInterface("org.kde.kglobalaccel","/kglobalaccel","org.kde.KGlobalAccel", QDBusConnection::sessionBus(), this);
        m_kwinInter = new QDBusInterface("org.kde.KWin","/KWin","org.kde.KWin", QDBusConnection::sessionBus(), this);
    }
}

void LockWorker::initConfiguration()
{
#ifndef ENABLE_DSS_SNIPE
    m_model->setAlwaysShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Ondemand);
#else
    m_model->setAlwaysShowUserSwitchButton(getDconfigValue("switchUser", Ondemand).toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getDconfigValue("switchUser", Ondemand).toInt() == AuthInterface::Ondemand);
#endif

    m_model->setGsCheckpwd(isCheckPwdBeforeRebootOrShut());

    checkPowerInfo();
}

/**
 * @brief 处理认证状态
 *
 * @param type      认证类型
 * @param state     认证状态
 * @param message   认证消息
 */
void LockWorker::onAuthStateChanged(const int type, const int state, const QString &message)
{
    qCInfo(DDE_SHELL) << "Auth type:" << type
            << ", authentication state:" << state
            << ", message:" << message;

    if (m_model->getAuthProperty().MFAFlag) {
        if (type == AT_All) {
            switch (state) {
            case AS_Success:
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                destroyAuthentication(m_account);
                onUnlockFinished(true);
                m_resetSessionTimer->stop();
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
                if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                    && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }

                // 处于序列化多因认证过程中时不要重置认证
                if (MFASequenceControl::instance().currentAuthType() == AuthType::AT_None) {
                    m_resetSessionTimer->start();
                }

                m_model->updateAuthState(AuthType(type), AuthState(state), message);
                break;
            case AS_Failure:
                if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                    && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
                endAuthentication(m_account, AuthType(type));
                if (!m_model->currentUser()->limitsInfo(type).locked
                        && type != AT_Face && type != AT_Iris) {
                    QTimer::singleShot(50, this, [this, type] {
                        startAuthentication(m_account, AuthType(type));
                    });
                }
                QTimer::singleShot(50, this, [this, type, state, message] {
                    m_model->updateAuthState(AuthType(type), AuthState(state), message);
                });
                break;
            case AS_Locked:
                if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                    && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                endAuthentication(m_account, AuthType(type));
                // TODO: 信号时序问题,考虑优化,Bug 89056
                QTimer::singleShot(50, this, [this, type, state, message] {
                    m_model->updateAuthState(AuthType(type), AuthState(state), message);
                });
                break;
            case AS_Timeout:
            case AS_Error:
                qCWarning(DDE_SHELL) << "Auth error, type: " << type << ", state: " << state << ", message: " << message;
                endAuthentication(m_account, AuthType(type));
                m_model->updateAuthState(AuthType(type), AuthState(state), message);
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
            && m_model->currentModeState() != SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        }
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
            // 单因失败会返回明确的失败类型，不关注type为-1的情况
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

void LockWorker::doPowerAction(const SessionBaseModel::PowerAction action)
{
#ifndef ENABLE_DSS_SNIPE
    bool sleepLock = m_model->getPowerGSettings("", "sleepLock").toBool();
#else
    bool sleepLock = isSleepLock();
#endif
    qCInfo(DDE_SHELL) << "Do power action:" << action;
    switch (action) {
    case SessionBaseModel::PowerAction::RequireSuspend:
    {
        m_model->setIsBlackMode(true);

        int delayTime = 500;
#ifndef ENABLE_DSS_SNIPE
        if(m_gsettings && m_gsettings->keys().contains("delaytime")){
            delayTime = m_gsettings->get("delaytime").toInt();
            qCInfo(DDE_SHELL) << "DelayTime : " << delayTime;
        }
#else
        if (m_dConfig) {
            delayTime = m_dConfig->value("delayTime", 500).toInt();
            qInfo() << "delayTime : " << delayTime;
        }
#endif
        if (delayTime < 0) {
            delayTime = 500;
        }
        WarningContent::instance()->tryGrabKeyboard();
        QTimer::singleShot(delayTime, this, [=] {
            // 待机休眠前设置Locked为true,避免刚唤醒时locked状态不对
            if (sleepLock) {
                setLocked(true);
            }
            m_sessionManagerInter->RequestSuspend();
        });
    }
        break;
    case SessionBaseModel::PowerAction::RequireHibernate:
    {
        m_model->setIsBlackMode(true);

        int delayTime = 500;
#ifndef ENABLE_DSS_SNIPE
        if(m_gsettings && m_gsettings->keys().contains("delaytime")){
            delayTime = m_gsettings->get("delaytime").toInt();
            qCInfo(DDE_SHELL) << "DelayTime : " << delayTime;
        }
#else
        if (m_dConfig) {
            delayTime = m_dConfig->value("delayTime", 500).toInt();
            qInfo() << "delayTime : " << delayTime;
        }
#endif
        if (delayTime < 0) {
            delayTime = 500;
        }
        if (canHibernate()){
            WarningContent::instance()->tryGrabKeyboard();
            QTimer::singleShot(delayTime, this, [=] {
                // 待机休眠前设置Locked为true,避免刚唤醒时locked状态不对
                if (sleepLock) {
                    setLocked(true);
                }
                m_sessionManagerInter->RequestHibernate();
            });
        }
    }
        break;
    case SessionBaseModel::PowerAction::RequireRestart: {
        m_model->setShutdownMode(true);
        auto gsCheckPwd = m_model->gsCheckpwd();
        if (!isLocked() || m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode || !gsCheckPwd) {
            m_sessionManagerInter->RequestReboot();
        } else {
            createAuthentication(m_account);
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
        }

        if (m_model->visibleShutdownWhenRebootOrShutdown()) {
            return;
        }

        if (!gsCheckPwd)
            m_model->setVisible(false);

        return;
    }
    case SessionBaseModel::PowerAction::RequireShutdown: {
        m_model->setShutdownMode(true);
        auto gsCheckPwd = m_model->gsCheckpwd();
        if (!isLocked() || m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode || !gsCheckPwd) {
            m_sessionManagerInter->RequestShutdown();
        } else {
            createAuthentication(m_account);
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
        }

        if (m_model->visibleShutdownWhenRebootOrShutdown()) {
            return;
        }

        if (!gsCheckPwd)
            m_model->setVisible(false);

        return;
    }
    case SessionBaseModel::PowerAction::RequireLock:
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        createAuthentication(m_model->currentUser()->name());
        break;
    case SessionBaseModel::PowerAction::RequireLogout:
        m_sessionManagerInter->RequestLogout();
        return;
    case SessionBaseModel::PowerAction::RequireSwitchSystem:
        m_switchosInterface->setOsFlag(!m_switchosInterface->getOsFlag());
        QTimer::singleShot(200, this, [this] { m_sessionManagerInter->RequestReboot(); });
        break;
    case SessionBaseModel::PowerAction::RequireSwitchUser:
        if (m_model->userlistVisible()) {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
        } else {
            std::shared_ptr<User> user_ptr = m_model->findUserByName("...");
            if (user_ptr) {
                switchToUser(user_ptr);
            }
        }
        break;
    case SessionBaseModel::PowerAction::RequireUpdateRestart:
        UpdateWorker::instance()->doUpdate(false);
        break;
    case SessionBaseModel::PowerAction::RequireUpdateShutdown:
        UpdateWorker::instance()->doUpdate(true);
        break;
    default:
        break;
    }

    m_model->setPowerAction(SessionBaseModel::PowerAction::None);
}


bool LockWorker::isCheckPwdBeforeRebootOrShut()
{
#ifndef ENABLE_DSS_SNIPE
    if (QGSettings::isSchemaInstalled("com.deepin.dde.session-shell")) {
        QGSettings updateSettings("com.deepin.dde.session-shell", QByteArray(), this);
        if (updateSettings.keys().contains("checkpwd")) {
             return updateSettings.get("checkpwd").toBool();
        }
    }
#else
    if (m_dConfig) {
        return m_dConfig->value("checkpwd", false).toBool();
    }
#endif
    return false;
}

/**
 * @brief 将当前用户的信息保存到 LockService 服务
 *
 * @param user
 */
void LockWorker::setCurrentUser(const std::shared_ptr<User> user)
{
    qCInfo(DDE_SHELL) << "Set current user: " << user->name();
    QJsonObject json;
    json["AuthType"] = static_cast<int>(user->lastAuthType());
    json["Name"] = user->name();
    json["Type"] = user->type();
    json["Uid"] = static_cast<int>(user->uid());
    json["LastCustomAuth"] = user->lastCustomAuth(); // 上一次认证通过自定义认证类型
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact)));
}

void LockWorker::switchToUser(std::shared_ptr<User> user)
{
    // 发生用户切换时锁住
    setLocked(true);

    qCDebug(DDE_SHELL) << "LockWorker::switchToUser:" << m_account << user->name();
    if (user->name() == m_account || *user == *m_model->currentUser()) {
        qCInfo(DDE_SHELL) << "Switch to current user:" << user->name() << user->isLogin();
        createAuthentication(user->name());
        return;
    } else {
        qCInfo(DDE_SHELL) << "Switch user from" << m_account << "to" << user->name() << ", login: " << user->isLogin();
        endAuthentication(m_account, AT_All);
        setCurrentUser(user);
    }

    /**
     * 切换用户时，需要先将当前用户的界面切换为密码输入框，再切换为下一用户。
     * 当前用户界面还未更新完成，已经切换为下一用户界面了，导致切换回来时，闪现用户列表。
     * 故使用 QTimer 将切换用户的操作放在事件队列最后处理。
     */
    if (user->isLogin()) {
        QTimer::singleShot(0, this, [user] {
            QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
        });
    } else {
        QTimer::singleShot(0, this, [] {
            QProcess::startDetached("dde-switchtogreeter", QStringList());
        });
    }
}

void LockWorker::enableZoneDetected(bool disable)
{
    m_hotZoneInter->EnableZoneDetected(disable);
}

/**
 * @brief 获取当前 Session 是否被锁定
 *
 * @return true
 * @return false
 */
bool LockWorker::isLocked() const
{
    return m_sessionManagerInter->locked();
}

/**
 * @brief 设置 Locked 的状态
 *
 * @param locked
 */
void LockWorker::setLocked(const bool locked)
{
#ifdef QT_DEBUG
    Q_UNUSED(locked)
#else
    m_sessionManagerInter->SetLocked(locked);
#endif
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void LockWorker::createAuthentication(const QString &account)
{
    qCInfo(DDE_SHELL) << "Account:" << account;
    QString userPath = m_accountsInter->FindUserByName(account);
    if (!userPath.startsWith("/")) {
        qCWarning(DDE_SHELL) << "User's path is invalid:" << userPath;
        return;
    }

    // 如果验证会话已经存在，销毁后重新开启验证
    if (m_authFramework->authSessionExist(account) || m_authFramework->IsUsingPamAuth()) {
        endAuthentication(account, AT_All);
        destroyAuthentication(account);
    }

    // 同步密码过期的信息
    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    if (user_ptr) {
        user_ptr->updatePasswordExpiredInfo();

        if (user_ptr->isNoPasswordLogin()) {
            qCInfo(DDE_SHELL) << "User is no password login";
            // 无密码登录锁定后也属于锁屏，需要设置lock属性
#ifndef ENABLE_DSS_SNIPE
            if (m_model->getPowerGSettings("", "sleepLock").toBool()) {
#else
            if (isSleepLock()) {
#endif
                setLocked(true);
            } else if (m_model->isQuickLoginProcess()) {
                //若此次程序拉起为快速登录流程，需要锁屏
                qCInfo(DDE_SHELL) << "Quick login requires screen lock";
                setLocked(true);
                m_model->setQuickLoginProcess(false);
            }

            return;
        }
    }

    m_account = account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), Lock);
        break;
    default:
        m_authFramework->CreateAuthenticate(account);
        m_model->setAuthType(AT_PAM);
        break;
    }

    setLocked(true);
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void LockWorker::destroyAuthentication(const QString &account)
{
    qCInfo(DDE_SHELL) << "Destroy authentication, account:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->DestroyAuthController(account);
        break;
    default:
        m_authFramework->DestroyAuthenticate();
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
void LockWorker::startAuthentication(const QString &account, const AuthFlags authType)
{
    qCInfo(DDE_SHELL) << "Start authentication, account:" << account << ", auth types:" << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->StartAuthentication(account, authType, -1);
        break;
    default:
        m_authFramework->CreateAuthenticate(account);
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
void LockWorker::sendTokenToAuth(const QString &account, const AuthType authType, const QString &token)
{
    qCInfo(DDE_SHELL) << "Send token to auth, account: " << account << ", auth type: " << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->SendTokenToAuth(account, authType, token);
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
void LockWorker::endAuthentication(const QString &account, const AuthFlags authType)
{
    qCInfo(DDE_SHELL) << "End authentication, account:" << account << ", auth type:" << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->EndAuthentication(account, authType);
        break;
    default:
        break;
    }
}

void LockWorker::onEndAuthentication(const QString &account, const AuthFlags authType)
{
    endAuthentication(account, authType);
    if (AT_All == authType) {
        destroyAuthentication(account);
    }
}

void LockWorker::lockServiceEvent(quint32 eventType, quint32 pid, const QString &username, const QString &message)
{
    Q_UNUSED(pid)
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
        emit m_model->authFailedMessage(tr("Fingerprint verification timed out, please enter your password manually"));
        return;
    }

    switch (eventType) {
    case DBusLockService::PromptQuestion:
        qCWarning(DDE_SHELL) << "Prompt question from pam: " << message;
        emit m_model->authFailedMessage(message);
        break;
    case DBusLockService::PromptSecret:
        qCWarning(DDE_SHELL) << "Prompt secret from pam: " << message;
        if (m_isThumbAuth && !msg.isEmpty()) {
            emit m_model->authFailedMessage(msg);
        }
        break;
    case DBusLockService::ErrorMsg:
        qCWarning(DDE_SHELL) << "Error message from pam: " << message;
        if (msg == "Failed to match fingerprint") {
            emit m_model->authFailedTipsMessage(tr("Failed to match fingerprint"));
            emit m_model->authFailedMessage("");
        }
        break;
    case DBusLockService::TextInfo:
        emit m_model->authFailedMessage(QString(dgettext("fprintd", message.toLatin1())));
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

void LockWorker::onAuthFinished()
{
    onUnlockFinished(true);
    setCurrentUser(m_model->currentUser());
}

void LockWorker::onUnlockFinished(bool unlocked)
{
    qCInfo(DDE_SHELL) << "Unlock finished, is unlocked: " << unlocked;

    m_authenticating = false;

    //To Do: 最好的方案是修改同步后端认证信息的代码设计
    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::UserMode)
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
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

    setLocked(!unlocked);
    emit m_model->authFinished(unlocked);
}

void LockWorker::restartResetSessionTimer()
{
    if (m_model->visible() && m_resetSessionTimer->isActive()) {
        m_resetSessionTimer->start();
    }
}

void LockWorker::disableGlobalShortcutsForWayland(const bool enable)
{
    qCInfo(DDE_SHELL) << "Disable global shortcuts for wayland: " << enable;
    if (m_kwinInter == nullptr || m_kglobalaccelInter == nullptr) {
        return;
    }
    if (!m_kwinInter->isValid()) {
        qCWarning(DDE_SHELL) << "Kwin interface is not valid";
        return;
    }
    QDBusReply<void> reply = m_kwinInter->call("disableGlobalShortcutsForClient", enable);
    if (!reply.isValid()) {
        qCWarning(DDE_SHELL) << "Call `disableGlobalShortcutsForClient` failed, error: " << reply.error();
    }
    if (!m_kglobalaccelInter->isValid()) {
        qCWarning(DDE_SHELL) << "Global accel interface is not valid";
        return;
    }
    const QStringList &shortCutList = DConfigHelper::instance()
                                              ->getConfig("enableShortcutForLock", QStringList())
                                              .toStringList();
    if (!shortCutList.isEmpty()) {
        foreach (const QString &shortcut, shortCutList) {
            reply = m_kglobalaccelInter->call("setActiveByUniqueName", shortcut, true);
            if (!reply.isValid()) {
                qCWarning(DDE_SHELL) << "Call `setActiveByUniqueName` failed, error: " << reply.error();
            }
        }
    } else {
        qCWarning(DDE_SHELL) << "There is no shortcut should be enabled!";
    }
}

void LockWorker::checkAccount(const QString &account, bool switchUser)
{
    Q_UNUSED(account)
    Q_UNUSED(switchUser)

    if (m_model->currentUser() && m_model->currentUser()->isNoPasswordLogin()) {
        qCInfo(DDE_SHELL) << "Current user has set 'no password login' : " << account;

        authFinishedAction();
    }
}

void LockWorker::authFinishedAction()
{
    if ((m_model->powerAction() == SessionBaseModel::PowerAction::RequireRestart
            || m_model->powerAction() == SessionBaseModel::PowerAction::RequireShutdown)
            && WarningContent::instance()->supportDelayOrWait()) {
        qCDebug(DDE_SHELL) << "return to warningcontent";
        FullScreenBackground::setContent(WarningContent::instance());
        m_model->setCurrentContentType(SessionBaseModel::WarningContent);
    } else {
        qCDebug(DDE_SHELL) << "hide main window";
        m_model->setVisible(false);
    }
    onAuthFinished();
    enableZoneDetected(true);
}

void LockWorker::onNoPasswordLoginChanged(const QString &account, bool noPassword)
{
    qCDebug(DDE_SHELL) << "user:" << account << ", noPassword:" << noPassword << ", visible :" << m_model->visible();
    if(m_model->currentUser()->name() == account && m_model->visible()) {
        if (m_model->currentModeState() != SessionBaseModel::ShutDownMode && m_model->currentModeState() != SessionBaseModel::UserMode) {
            createAuthentication(account);
        }
    }
}

void LockWorker::sendExtraInfo(const QString &account, AuthCommon::AuthType authType, const QString &info)
{
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->sendExtraInfo(account, authType, info);
        break;
    default:
        break;
    }
}
