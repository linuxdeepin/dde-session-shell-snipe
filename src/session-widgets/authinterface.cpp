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

static std::pair<bool, qint64> checkIsPartitionType(const QStringList &list)
{
    std::pair<bool, qint64> result{ false, -1 };

    if (list.length() != 5) {
        return result;
    }

    const QString type{ list[1] };
    const QString size{ list[2] };

    result.first  = type == "partition";
    result.second = size.toLongLong() * 1024.0f;

    return result;
}

static qint64 get_power_image_size()
{
    qint64 size{ 0 };
    QFile  file("/sys/power/image_size");

    if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
        size = file.readAll().trimmed().toLongLong();
        file.close();
    }

    return size;
}

AuthInterface::AuthInterface(SessionBaseModel *const model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_accountsInter(new AccountsInter(ACCOUNT_DBUS_SERVICE, ACCOUNT_DBUS_PATH, QDBusConnection::systemBus(), this))
    , m_loginedInter(new LoginedInter(ACCOUNT_DBUS_SERVICE, "/com/deepin/daemon/Logined", QDBusConnection::systemBus(), this))
    , m_login1Inter(new DBusLogin1Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this))
    , m_powerManagerInter(new PowerManagerInter("com.deepin.daemon.PowerManager", "/com/deepin/daemon/PowerManager", QDBusConnection::systemBus(), this))
    , m_authenticateInter(new Authenticate("com.deepin.daemon.Authenticate",
                                           "/com/deepin/daemon/Authenticate",
                                           QDBusConnection::systemBus(),
                                           this))
    , m_lastLogoutUid(INT_MAX)
    , m_loginUserList(0)
{
    if (m_login1Inter->isValid()) {
       QString session_self = m_login1Inter->GetSessionByPID(0).value().path();
       m_login1SessionSelf = new Login1SessionSelf("org.freedesktop.login1", session_self, QDBusConnection::systemBus(), this);
       m_login1SessionSelf->setSync(false);
    } else {
        qWarning() << "m_login1Inter:" << m_login1Inter->lastError().type();
    }

    if (m_model->currentType() != SessionBaseModel::LightdmType && QGSettings::isSchemaInstalled("com.deepin.dde.sessionshell.control")) {
        m_gsettings = new QGSettings("com.deepin.dde.sessionshell.control", "/com/deepin/dde/sessionshell/control/", this);
    }
}

void AuthInterface::setLayout(std::shared_ptr<User> user, const QString &layout)
{
    user->setCurrentLayout(layout);
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
    user_ptr->setisLogind(isLogined(user_ptr->uid()));
    m_model->userAdd(user_ptr);
}

void AuthInterface::onUserRemove(const QString &user)
{
    QList<std::shared_ptr<User>> list = m_model->userList();

    for (auto u : list) {
        if (u->path() == user && u->type() == User::Native) {
            m_model->userRemoved(u);
            break;
        }
    }
}

void AuthInterface::initData()
{
    m_accountsInter->userList();
    m_loginedInter->lastLogoutUser();

    checkConfig();
    checkPowerInfo();
}

void AuthInterface::initDBus()
{
    m_accountsInter->setSync(false);
    m_loginedInter->setSync(false);

    connect(m_accountsInter, &AccountsInter::UserListChanged, this, &AuthInterface::onUserListChanged, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserAdded, this, &AuthInterface::onUserAdded, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserDeleted, this, &AuthInterface::onUserRemove, Qt::QueuedConnection);

    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, this, &AuthInterface::onLastLogoutUserChanged);
    connect(m_loginedInter, &LoginedInter::UserListChanged, this, &AuthInterface::onLoginUserListChanged);

    connect(m_authenticateInter, &Authenticate::LimitUpdated, this, [this] (const QString& name) {
        auto user = m_model->findUserByName(name);
        updateLockLimit(user);
    });
}

void AuthInterface::onLastLogoutUserChanged(uint uid)
{
    m_lastLogoutUid = uid;

    QList<std::shared_ptr<User>> userList = m_model->userList();
    for (auto it = userList.constBegin(); it != userList.constEnd(); ++it) {
        if ((*it)->uid() == uid) {
            m_model->setLastLogoutUser((*it));
            return;
        }
    }

    m_model->setLastLogoutUser(std::shared_ptr<User>(nullptr));
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
            [=](const uint find_addomain_uid) { return find_addomain_uid == uid; });

        if (haveDisplay && find_it == availableUidList.end()) {
            // init addoman user
            std::shared_ptr<User> u(new ADDomainUser(uid));
            u->setisLogind(true);

            struct passwd *pws;
            pws = getpwuid(uid);

            static_cast<ADDomainUser *>(u.get())->setUserDisplayName(pws->pw_name);
            static_cast<ADDomainUser *>(u.get())->setUserName(pws->pw_name);

            if (uid == m_currentUserUid && m_model->currentUser().get() == nullptr) {
                m_model->setCurrentUser(u);
            }
            m_model->userAdd(u);
            availableUidList.push_back(uid);
        }
    }

    QList<std::shared_ptr<User>> uList = m_model->userList();
    for (auto it = uList.begin(); it != uList.end();) {
        std::shared_ptr<User> user = *it;

        auto find_it =
            std::find_if(m_loginUserList.begin(), m_loginUserList.end(),
                         [=](const uint find_uid) { return find_uid == user->uid(); });

        if (find_it == m_loginUserList.end() &&
            (user->type() == User::ADDomain && !user->isDoMainUser())) { //特殊账号入口不能被删除了
            m_model->userRemoved(user);
            it = uList.erase(it);
        }
        else {
            user->setisLogind(isLogined(user->uid()));
            ++it;
        }
    }

    if(m_model->isServerModel())
        emit m_model->userListLoginedChanged(m_model->logindUser());
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

void AuthInterface::updateLockLimit(std::shared_ptr<User> user)
{
    if (user == nullptr && user->name().isEmpty())
        return;

    QDBusPendingCall call = m_authenticateInter->GetLimits(user->name());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [ = ] {
        if (!call.isError()) {
            QDBusReply<QString> reply = call.reply();
            QJsonArray arr = QJsonDocument::fromJson(reply.value().toUtf8()).array();
            for (QJsonValue val : arr) {
               QJsonObject obj = val.toObject();
               if (obj["type"].toString() == "password") {
                   bool is_lock = obj["locked"].toBool();
                   // cur_time 当前时间，interval_time 间隔时间，rest_time 除分钟外多余的秒数，lock_time 分钟数
                   uint cur_time = QDateTime::currentDateTime().toTime_t();
                   uint interval_time = timeFromString(obj["unlockTime"].toString()) - cur_time;
                   uint rest_time = interval_time % 60;
                   uint lock_time = (interval_time + 59) / 60;
                   user->updateLockLimit(is_lock, lock_time, rest_time);
               }
            }
        } else {
            qWarning() << "get Limits error: " << call.error().message();
        }

        watcher->deleteLater();
    });
}

bool AuthInterface::isLogined(uint uid)
{
    auto isLogind = std::find_if(m_loginUserList.begin(), m_loginUserList.end(),
                                 [=](uint UID) { return uid == UID; });

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

void AuthInterface::checkSwap()
{
    QFile file("/proc/swaps");
    if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
        bool           hasSwap{ false };
        const QString &body = file.readAll();
        QTextStream    stream(body.toUtf8());
        while (!stream.atEnd()) {
            const std::pair<bool, qint64> result =
                checkIsPartitionType(stream.readLine().simplified().split(
                    " ", QString::SplitBehavior::SkipEmptyParts));
            qint64 image_size{ get_power_image_size() };

            if (result.first) {
                hasSwap = image_size < result.second;
            }

            qDebug() << "check using swap partition!";
            qDebug() << QString("image_size: %1, swap partition size: %2")
                            .arg(QString::number(image_size))
                            .arg(QString::number(result.second));

            if (hasSwap) {
                break;
            }
        }

        m_model->setHasSwap(hasSwap);
        file.close();
    }
    else {
        qWarning() << "open /proc/swaps failed! please check permission!!!";
    }
}
