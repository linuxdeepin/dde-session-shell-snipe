#include "authinterface.h"
#include "sessionbasemodel.h"
#include "userinfo.h"

#include <grp.h>
#include <libintl.h>
#include <pwd.h>
#include <unistd.h>
#include <QProcessEnvironment>

#define POWER_CAN_SLEEP "POWER_CAN_SLEEP"
#define POWER_CAN_HIBERNATE "POWER_CAN_HIBERNATE"

using namespace Auth;

AuthInterface::AuthInterface(SessionBaseModel *const model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_accountsInter(new AccountsInter("com.deepin.daemon.Accounts", "/com/deepin/daemon/Accounts", QDBusConnection::systemBus(), this))
    , m_loginedInter(new LoginedInter("com.deepin.daemon.Accounts", "/com/deepin/daemon/Logined", QDBusConnection::systemBus(), this))
    , m_login1Inter(new DBusLogin1Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this))
    , m_powerManagerInter(new PowerManagerInter("com.deepin.daemon.PowerManager", "/com/deepin/daemon/PowerManager", QDBusConnection::systemBus(), this))
    , m_authenticateInter(new Authenticate("com.deepin.daemon.Authenticate", "/com/deepin/daemon/Authenticate", QDBusConnection::systemBus(), this))
    , m_dbusInter(new DBusObjectInter("org.freedesktop.DBus", "/org/freedesktop/DBus", QDBusConnection::systemBus(), this))
    , m_lastLogoutUid(0)
    , m_loginUserList(0)
{
    if (m_login1Inter->isValid()) {
       QString session_self = m_login1Inter->GetSessionByPID(0).value().path();
       m_login1SessionSelf = new Login1SessionSelf("org.freedesktop.login1", session_self, QDBusConnection::systemBus(), this);
       m_login1SessionSelf->setSync(false);
    } else {
        qWarning() << "m_login1Inter:" << m_login1Inter->lastError().type();
    }

    if (m_model->currentType() != SessionBaseModel::LightdmType && QGSettings::isSchemaInstalled("com.deepin.dde.session-shell")) {
        m_gsettings = new QGSettings("com.deepin.dde.session-shell", "/com/deepin/dde/session-shell/", this);
    }
}

void AuthInterface::setLayout(std::shared_ptr<User> user, const QString &layout)
{
    user->setKeyboardLayout(layout);
}

void AuthInterface::onUserListChanged(const QStringList &list)
{
    m_model->setUserListSize(list.size());

    QStringList tmpList;
    for (std::shared_ptr<User> u : m_model->userList()) {
        if (u->type() == User::Native)
            tmpList << ACCOUNTS_DBUS_PREFIX + QString::number(u->uid());
    }

    for (const QString &u : list) {
        if (!tmpList.contains(u)) {
            tmpList << u;
            onUserAdded(u);
        }
    }

    for (const QString &u : tmpList) {
        if (!list.contains(u)) {
            onUserRemove(u);
        }
    }

    m_loginedInter->userList();
}

void AuthInterface::onUserAdded(const QString &user)
{
    std::shared_ptr<User> user_ptr(new NativeUser(user));
    user_ptr->updateLoginStatus(isLogined(user_ptr->uid()));
    m_model->addUser(user_ptr);
}

void AuthInterface::onUserRemove(const QString &user)
{
    QList<std::shared_ptr<User>> list = m_model->userList();

    for (auto u : list) {
        if (u->path() == user && u->type() == User::Native) {
            m_model->removeUser(u);
            break;
        }
    }
}

void AuthInterface::initData()
{
    onUserListChanged(m_accountsInter->userList());
    onLastLogoutUserChanged(m_loginedInter->lastLogoutUser());
    onLoginUserListChanged(m_loginedInter->userList());
    // m_accountsInter->userList();
    // m_loginedInter->lastLogoutUser();

    checkConfig();
    checkPowerInfo();
}

void AuthInterface::initDBus()
{
    // m_accountsInter->setSync(false);
    // m_loginedInter->setSync(false);

    connect(m_accountsInter, &AccountsInter::UserListChanged, this, &AuthInterface::onUserListChanged, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserAdded, this, &AuthInterface::onUserAdded, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserDeleted, this, &AuthInterface::onUserRemove, Qt::QueuedConnection);

    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, this, &AuthInterface::onLastLogoutUserChanged);
    connect(m_loginedInter, &LoginedInter::UserListChanged, this, &AuthInterface::onLoginUserListChanged);
}

void AuthInterface::onLastLogoutUserChanged(uint uid)
{
    m_lastLogoutUid = uid;

    QList<std::shared_ptr<User>> userList = m_model->userList();
    for (auto it = userList.constBegin(); it != userList.constEnd(); ++it) {
        if ((*it)->uid() == uid) {
            m_model->updateLastLogoutUser((*it));
            return;
        }
    }

    m_model->updateLastLogoutUser(std::shared_ptr<User>(nullptr));
}

void AuthInterface::onLoginUserListChanged(const QString &list)
{
    m_loginUserList.clear();

    std::list<uint> availableUidList;
    for (std::shared_ptr<User> user : m_model->userList()) {
        availableUidList.push_back(user->uid());
    }

    QJsonObject userList = QJsonDocument::fromJson(list.toUtf8()).object();
    for (auto it = userList.constBegin(); it != userList.constEnd(); ++it) {
        const bool haveDisplay = checkHaveDisplay(it.value().toArray());
        const uint uid         = it.key().toUInt();

        // skip not have display users
        if (haveDisplay) {
            m_loginUserList.push_back(uid);
        }

        auto find_it = std::find_if(
            availableUidList.begin(), availableUidList.end(),
            [=] (const uint find_addomain_uid) { return find_addomain_uid == uid; });

        if (haveDisplay && find_it == availableUidList.end()) {
            // init addoman user
            std::shared_ptr<User> u(new ADDomainUser(uid));
            u->updateLoginStatus(true);

            if (uid == m_currentUserUid && m_model->currentUser().get() == nullptr) {
                m_model->updateCurrentUser(u);
            }
            m_model->addUser(u);
            availableUidList.push_back(uid);
        }
    }

    QList<std::shared_ptr<User>> uList = m_model->userList();
    for (auto it = uList.begin(); it != uList.end();) {
        std::shared_ptr<User> user = *it;
        user->updateLoginStatus(isLogined(user->uid()));
        ++it;
    }

    if (m_model->isServerModel()) {
        emit m_model->userListLoginedChanged(m_model->loginedUserList());
    }
}

bool AuthInterface::checkHaveDisplay(const QJsonArray &array)
{
    for (auto it = array.constBegin(); it != array.constEnd(); ++it) {
        const QJsonObject &obj = (*it).toObject();

        // If user without desktop or display, this is system service, need skip.
        if (!obj["Display"].toString().isEmpty() &&
            !obj["Desktop"].toString().isEmpty()) {
            return true;
        }
    }

    return false;
}

QVariant AuthInterface::getGSettings(const QString& node, const QString& key)
{
    QVariant value = valueByQSettings<QVariant>(node, key, true);
    if (m_gsettings != nullptr && m_gsettings->keys().contains(key)) {
        value = m_gsettings->get(key);
    }
    return value;
}

bool AuthInterface::isLogined(uint uid)
{
    auto isLogind = std::find_if(m_loginUserList.begin(), m_loginUserList.end(),
                                 [=] (uint UID) { return uid == UID; });

    return isLogind != m_loginUserList.end();
}

bool AuthInterface::isDeepin()
{
    // 这是临时的选项，只在Deepin下启用同步认证功能，其他发行版下禁用。
#ifdef QT_DEBUG
    return true;
#else
    return getGSettings("","useDeepinAuth").toBool();
#endif
}

void AuthInterface::checkConfig()
{
    m_model->setAlwaysShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("","switchuser").toInt() == AuthInterface::Ondemand);
}

void AuthInterface::checkPowerInfo()
{
    //替换接口org.freedesktop.login1 为com.deepin.sessionManager,原接口的是否支持待机和休眠的信息不准确
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    bool can_sleep = env.contains(POWER_CAN_SLEEP) ? QVariant(env.value(POWER_CAN_SLEEP)).toBool()
                                                   : getGSettings("Power","sleep").toBool() && m_powerManagerInter->CanSuspend();
    m_model->setCanSleep(can_sleep);

    bool can_hibernate = env.contains(POWER_CAN_HIBERNATE) ? QVariant(env.value(POWER_CAN_HIBERNATE)).toBool()
                                                           : getGSettings("Power","hibernate").toBool() && m_powerManagerInter->CanHibernate();

    m_model->setHasSwap(can_hibernate);
}
