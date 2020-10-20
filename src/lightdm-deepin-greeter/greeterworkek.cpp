#include "greeterworkek.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/userinfo.h"
#include "src/global_util/keyboardmonitor.h"

#include <libintl.h>
#include <DSysInfo>

#define LOCKSERVICE_PATH "/com/deepin/dde/LockService"
#define LOCKSERVICE_NAME "com.deepin.dde.LockService"

using namespace Auth;
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

    bool get(const bool defaultValue) { return m_settings.value(m_username, defaultValue).toBool(); }
    void set(const bool value) { m_settings.setValue(m_username, value); }

private:
    QString m_username;
    QSettings m_settings;
};

GreeterWorkek::GreeterWorkek(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_greeter(new QLightDM::Greeter(this))
    , m_lockInter(new DBusLockService(LOCKSERVICE_NAME, LOCKSERVICE_PATH, QDBusConnection::systemBus(), this))
    , m_AuthenticateInter(new Authenticate(AuthenticateService,
                                         "/com/deepin/daemon/Authenticate",
                                         QDBusConnection::systemBus(), this))
    , m_authenticating(false)
    , m_password(QString())
{
    if (m_AuthenticateInter->isValid()) {
        m_AuthenticateInter->setTimeout(300);
    }

    if (!m_greeter->connectSync()) {
        qWarning() << "greeter connect fail !!!";
    }

    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &GreeterWorkek::prompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &GreeterWorkek::message);
    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &GreeterWorkek::authenticationComplete);

    connect(model, &SessionBaseModel::onPowerActionChanged, this, [ = ](SessionBaseModel::PowerAction poweraction) {
        switch (poweraction) {
        case SessionBaseModel::PowerAction::RequireShutdown:
            m_login1Inter->PowerOff(true);
            break;
        case SessionBaseModel::PowerAction::RequireRestart:
            m_login1Inter->Reboot(true);
            break;
        case SessionBaseModel::PowerAction::RequireSuspend:
            m_login1Inter->Suspend(true);
            break;
        case SessionBaseModel::PowerAction::RequireHibernate:
            m_login1Inter->Hibernate(true);
            break;
        default:
            break;
        }

        model->setPowerAction(SessionBaseModel::PowerAction::None);
    });

    connect(model, &SessionBaseModel::lockChanged, this, [ = ](bool lock) {
        if (!lock) {
            m_password.clear();
            resetLightdmAuth(m_model->currentUser(), 100, false);
        }
    });

    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [ = ](bool active) {
        if(active) {
            m_authenticating = false;
            resetLightdmAuth(m_model->currentUser(), 100, false);
        }
    });

    connect(KeyboardMonitor::instance(), &KeyboardMonitor::numlockStatusChanged, this, [ = ](bool on) {
        saveNumlockStatus(model->currentUser(), on);
    });

    connect(model, &SessionBaseModel::currentUserChanged, this, &GreeterWorkek::recoveryUserKBState);
    connect(m_lockInter, &DBusLockService::UserChanged, this, &GreeterWorkek::onCurrentUserChanged);

    const QString &switchUserButtonValue { valueByQSettings<QString>("Lock", "showSwitchUserButton", "ondemand") };
    m_model->setAlwaysShowUserSwitchButton(switchUserButtonValue == "always");
    m_model->setAllowShowUserSwitchButton(switchUserButtonValue == "ondemand");

    {
        initDBus();
        initData();

        if (QFile::exists("/etc/deepin/no_suspend"))
            m_model->setCanSleep(false);

        checkDBusServer(m_accountsInter->isValid());
        oneKeyLogin();
    }

    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || valueByQSettings<bool>("", "loginPromptInput", false)) {
        std::shared_ptr<User> user = std::make_shared<ADDomainUser>(INT_MAX);
        static_cast<ADDomainUser *>(user.get())->setUserDisplayName("...");
        static_cast<ADDomainUser *>(user.get())->setIsServerUser(true);
        m_model->setIsServerModel(true);
        m_model->userAdd(user);
        m_model->setCurrentUser(user);
    } else {
        connect(m_login1Inter, &DBusLogin1Manager::SessionRemoved, this, [ = ] {
            // lockservice sometimes fails to call on olar server
            QDBusPendingReply<QString> replay = m_lockInter->CurrentUser();
            replay.waitForFinished();

            if (!replay.isError()) {
                const QJsonObject obj = QJsonDocument::fromJson(replay.value().toUtf8()).object();
                auto user_ptr = m_model->findUserByUid(static_cast<uint>(obj["Uid"].toInt()));

                m_model->setCurrentUser(user_ptr);
                userAuthForLightdm(user_ptr);
            }
        });
    }
}

void GreeterWorkek::switchToUser(std::shared_ptr<User> user)
{
    qDebug() << "switch user from" << m_model->currentUser()->name() << " to "
             << user->name();

    // clear old password
    m_password.clear();
    m_authenticating = false;

    // just switch user
    if (user->isLogin()) {
        // switch to user Xorg
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    }

    QJsonObject json;
    json["Uid"] = static_cast<int>(user->uid());
    json["Type"] = user->type();
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();
}

void GreeterWorkek::authUser(const QString &password)
{
    if (m_authenticating) return;

    m_authenticating = true;

    // auth interface
    std::shared_ptr<User> user = m_model->currentUser();
    m_password = password;

    qDebug() << "greeter authenticate user: " << m_greeter->authenticationUser() << " current user: " << user->name();
    if (m_greeter->authenticationUser() != user->name()) {
        resetLightdmAuth(user, 100, false);
    }
    else {
        if (m_greeter->inAuthentication()) {
            m_greeter->respond(password);
            m_password.clear();
        }
        else {
            m_greeter->authenticate(user->name());
        }
    }
}

void GreeterWorkek::onUserAdded(const QString &user)
{
    std::shared_ptr<User> user_ptr(new NativeUser(user));

    user_ptr->setisLogind(isLogined(user_ptr->uid()));

    if (m_model->currentUser().get() == nullptr) {
        if (m_model->userList().isEmpty() || m_model->userList().first()->type() == User::ADDomain) {
            m_model->setCurrentUser(user_ptr);

            if (m_model->currentType() == SessionBaseModel::AuthType::LightdmType)
                userAuthForLightdm(user_ptr);
        }
    }

    if (user_ptr->uid() == m_lastLogoutUid) {
        m_model->setLastLogoutUser(user_ptr);
    }

    m_model->userAdd(user_ptr);
}

void GreeterWorkek::checkDBusServer(bool isvalid)
{
    if (isvalid) {
        onUserListChanged(m_accountsInter->userList());
    } else {
        // FIXME: 我不希望这样做，但是QThread::msleep会导致无限递归
        QTimer::singleShot(300, this, [ = ] {
            qWarning() << "com.deepin.daemon.Accounts is not start, rechecking!";
            checkDBusServer(m_accountsInter->isValid());
        });
    }
}

void GreeterWorkek::oneKeyLogin()
{
    // 多用户一键登陆
    auto user_firstlogin = m_AuthenticateInter->PreOneKeyLogin(AuthFlag::Fingerprint);
    user_firstlogin.waitForFinished();
    qDebug() << "GreeterWorkek::onFirstTimeLogin -- FirstTime Login User Name is : " << user_firstlogin;

    auto user_ptr = m_model->findUserByName(user_firstlogin);
    if (user_ptr.get() != nullptr && !user_firstlogin.isError()) {
        m_model->setCurrentUser(user_ptr);
        userAuthForLightdm(user_ptr);
    } else {
        onCurrentUserChanged(m_lockInter->CurrentUser());
    }
}

void GreeterWorkek::onCurrentUserChanged(const QString &user)
{
    const QJsonObject obj = QJsonDocument::fromJson(user.toUtf8()).object();
    m_currentUserUid = static_cast<uint>(obj["Uid"].toInt());

    for (std::shared_ptr<User> user_ptr : m_model->userList()) {
        if (!user_ptr->isLogin() && user_ptr->uid() == m_currentUserUid) {
            emit m_model->switchUserFinished();
            m_model->setCurrentUser(user_ptr);
            userAuthForLightdm(user_ptr);
            break;
        }
    }
}

void GreeterWorkek::userAuthForLightdm(std::shared_ptr<User> user)
{
    if (user.get() != nullptr && !user->isNoPasswdGrp()) {
        //后端需要大约600ms时间去释放指纹设备
        resetLightdmAuth(user, 100, true);
    }
}

void GreeterWorkek::prompt(QString text, QLightDM::Greeter::PromptType type)
{
    // Don't show password prompt from standard pam modules since
    // we'll provide our own prompt or just not.
    qDebug() << "pam prompt: " << text << type;

    const QString msg = text.simplified() == "Password:" ? "" : text;

    switch (type) {
    case QLightDM::Greeter::PromptTypeSecret:
        m_authenticating = false;

        if (msg.isEmpty()) break;

        if (!m_password.isEmpty()) {
            m_greeter->respond(m_password);
        } else {
            emit m_model->authFaildMessage(msg);
        }
        break;
    case QLightDM::Greeter::PromptTypeQuestion:
        emit m_model->authTipsMessage(text);
        break;
    }
}

// TODO(justforlxz): 错误信息应该存放在User类中, 切换用户后其他控件读取错误信息，而不是在这里分发。
void GreeterWorkek::message(QString text, QLightDM::Greeter::MessageType type)
{
    qDebug() << "pam message: " << text << type;

    switch (type) {
    case QLightDM::Greeter::MessageTypeInfo:
        qDebug() << Q_FUNC_INFO << "lightdm greeter message type info: " << text.toUtf8() << QString(dgettext("fprintd", text.toUtf8()));
        emit m_model->authFaildMessage(QString(dgettext("fprintd", text.toUtf8())));
        break;

    case QLightDM::Greeter::MessageTypeError:
        emit m_model->authFaildTipsMessage(QString(dgettext("fprintd", text.toUtf8())));
        break;
    }
}

void GreeterWorkek::authenticationComplete()
{
    qDebug() << "authentication complete, authenticated " << m_greeter->isAuthenticated();

    emit m_model->authFinished(m_greeter->isAuthenticated());

    if (!m_greeter->isAuthenticated()) {
        m_authenticating = false;
        if (m_password.isEmpty()) {
            resetLightdmAuth(m_model->currentUser(), 100, false);
            return;
        }

        m_password.clear();

        if (m_model->currentUser()->type() == User::Native) {
            emit m_model->authFaildTipsMessage(tr("Wrong Password"));
        }

        if (m_model->currentUser()->type() == User::ADDomain) {
            emit m_model->authFaildTipsMessage(tr("The account or password is not correct. Please enter again."));
        }

        if (m_model->currentUser()->isLockForNum()) {
            m_model->currentUser()->startLock();
            return;
        }

        resetLightdmAuth(m_model->currentUser(), 100, false);

        return;
    }

    m_model->currentUser()->resetLock();

    switch (m_model->powerAction()) {
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        return;
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        return;
    default: break;
    }

    qDebug() << "start session = " << m_model->sessionKey();

    auto startSessionSync = [ = ]() {
        QJsonObject json;
        json["Uid"]  = static_cast<int>(m_model->currentUser()->uid());
        json["Type"] = m_model->currentUser()->type();
        m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();

        m_greeter->startSessionSync(m_model->sessionKey());
        m_authenticating = false;
    };

    // NOTE(kirigaya): It is not necessary to display the login animation.
    emit requestUpdateBackground(m_model->currentUser()->desktopBackgroundPath());

#ifndef DISABLE_LOGIN_ANI
    QTimer::singleShot(1000, this, startSessionSync);
#else
    startSessionSync();
#endif
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
    if (user.get() == nullptr) return;

    const bool enabled = UserNumlockSettings(user->name()).get(false);

    qDebug() << "restore numlock status to " << enabled;

    // Resync numlock light with numlock status
    bool cur_numlock = KeyboardMonitor::instance()->isNumlockOn();
    KeyboardMonitor::instance()->setNumlockStatus(!cur_numlock);
    KeyboardMonitor::instance()->setNumlockStatus(cur_numlock);

    KeyboardMonitor::instance()->setNumlockStatus(enabled);
}

void GreeterWorkek::resetLightdmAuth(std::shared_ptr<User> user,int delay_time , bool is_respond)
{
    if (user->isLock()) {return;}

    QTimer::singleShot(delay_time, this, [ = ] {
        m_greeter->authenticate(user->name());
        if (is_respond) {
            m_greeter->respond(m_password);
        }
    });
}
