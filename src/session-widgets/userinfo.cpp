#include "userinfo.h"
#include "src/global_util/constants.h"

#include <unistd.h>
#include <pwd.h>
#include <grp.h>

QString userPwdName(__uid_t uid)
{
    //服务器版root用户的uid为0,需要特殊处理
    if (uid < 1000 && uid != 0) return QString();

    struct passwd *pw = nullptr;
    /* Fetch passwd structure (contains first group ID for user) */
    pw = getpwuid(uid);

    return QString().fromLocal8Bit(pw->pw_name);
}

static bool checkUserIsNoPWGrp(User const *user)
{
    if (user->type() == User::ADDomain) {
        return false;
    }

    // Caution: 32 here is unreliable, and there may be more
    // than this number of groups that the user joins.

    int ngroups = 32;
    gid_t groups[32];
    struct passwd *pw = nullptr;
    struct group *gr = nullptr;

    /* Fetch passwd structure (contains first group ID for user) */
    pw = getpwnam(user->name().toUtf8().data());
    if (pw == nullptr) {
        qDebug() << "fetch passwd structure failed, username: " << user->name();
        return false;
    }

    /* Retrieve group list */

    if (getgrouplist(user->name().toUtf8().data(), pw->pw_gid, groups, &ngroups) == -1) {
        fprintf(stderr, "getgrouplist() returned -1; ngroups = %d\n",
                ngroups);
        return false;
    }

    /* Display list of retrieved groups, along with group names */

    for (int i = 0; i < ngroups; i++) {
        gr = getgrgid(groups[i]);
        if (gr != nullptr && QString(gr->gr_name) == QString("nopasswdlogin")) {
            return true;
        }
    }

    return false;
}

static const QString toLocalFile(const QString &path)
{
    QUrl url(path);

    if (url.isLocalFile()) {
        return url.path();
    }

    return url.url();
}

User::User(QObject *parent)
    : QObject(parent)
    , m_isLogind(false)
    , m_locale(getenv("LANG"))
    , m_lockTimer(new QTimer)

{
    m_lockTimer->setInterval(1000 * 60);
    m_lockTimer->setSingleShot(false);
    connect(m_lockTimer.get(), &QTimer::timeout, this, &User::onLockTimeOut);
}

User::User(const User &user)
    : QObject(user.parent())
    , m_isLogind(user.m_isLogind)
    , m_uid(user.m_uid)
    , m_lockLimit(user.m_lockLimit)
    , m_userName(user.m_userName)
    , m_locale(user.m_locale)
    , m_lockTimer(user.m_lockTimer)
{

}

bool User::operator==(const User &user) const
{
    return type() == user.type() &&
           m_uid == user.m_uid;
}

void User::setLocale(const QString &locale)
{
    if (m_locale == locale) return;

    m_locale = locale;

    emit localeChanged(locale);
}

bool User::isNoPasswdGrp() const
{
    return checkUserIsNoPWGrp(this);
}

bool User::isUserIsvalid() const
{
    return true;
}

void User::setisLogind(bool isLogind)
{
    if (m_isLogind == isLogind) {
        return;
    }

    m_isLogind = isLogind;

    emit logindChanged(isLogind);
}

void User::setPath(const QString &path)
{
    if (m_path == path) return;

    m_path = path;
}

void User::onLockTimeOut()
{
    m_lockLimit.lockTime--;

    // 如果之前计时器读完秒，即间隔为所读秒数，则设置间隔为分钟
    if (m_lockTimer->interval() != 60)
        m_lockTimer->setInterval(1000 * 60);

    if (m_lockLimit.lockTime == 1)
        m_lockTimer->setInterval(1000 * 62);

    if(m_lockLimit.lockTime == 0) {
        m_lockLimit.isLock = false;
        m_lockTimer->stop();
        emit lockLimitFinished(false);
    }

    emit lockChanged(m_lockLimit.isLock);
}

void User::updateLockLimit(bool is_lock, uint lock_time, uint rest_second)
{
    m_lockLimit.lockTime = lock_time;
    m_lockLimit.isLock = is_lock;

    if (m_lockTimer->isActive()) {
         m_lockTimer->stop();
    }

    if (m_lockLimit.isLock) {
        //若锁定时间有整数分钟外的秒数，则计时器先读完秒，再设置间隔为分钟
        if (rest_second != 0) {
            m_lockTimer->setInterval(1000* (int(rest_second)));
        }
        m_lockTimer->start();
    }

    emit lockChanged(m_lockLimit.isLock);
}

void User::setUid(uid_t uuid)
{
    m_uid = uuid;
}

NativeUser::NativeUser(const QString &path, QObject *parent)
    : User(parent)
    , m_userInter(new UserInter(ACCOUNT_DBUS_SERVICE, path, QDBusConnection::systemBus(), this))
{
    m_userInter->setSync(false);
    connect(m_userInter, &UserInter::IconFileChanged, this, [ = ](const QString & avatar) {
        m_avatar = avatar;
        emit avatarChanged(avatar);
    });
    connect(m_userInter, &UserInter::UserNameChanged, this, [ = ](const QString & user_name) {
        m_userName = user_name;
        emit displayNameChanged(m_userName);
    });

    connect(m_userInter, &UserInter::UidChanged, this, [ = ](const QString & uid) {
        m_uid = uid.toUInt();
    });

    connect(m_userInter, &UserInter::DesktopBackgroundsChanged, this, [ = ] (const QStringList & paths) {
        QSettings settings(DDESESSIONCC::CONFIG_FILE + m_userName, QSettings::IniFormat);
        settings.beginGroup("User");

        int index = settings.value("Workspace").toInt();
        //刚安装的系统中返回的Workspace为0
        index = index <= 0 ? 1 : index;
        if (paths.size() >= index && index > 0) {
            m_desktopBackground = toLocalFile(paths.at(index - 1));
            emit desktopBackgroundPathChanged(m_desktopBackground);
        } else {
            qDebug() << "DesktopBackgroundsChanged get error index:" << index << ", paths:" << paths;
        }
    });

    connect(m_userInter, &UserInter::GreeterBackgroundChanged, this, [ = ](const QString & path) {
        m_greeterBackground = toLocalFile(path);
        emit greeterBackgroundPathChanged(m_greeterBackground);
    });

    connect(m_userInter, &UserInter::PasswordStatusChanged, this, [ = ](const QString& status){
        m_noPasswdGrp = (status == "NP" || checkUserIsNoPWGrp(this));
    });

    connect(m_userInter, &UserInter::LocaleChanged, this, &NativeUser::setLocale);
    connect(m_userInter, &UserInter::HistoryLayoutChanged, this, &NativeUser::kbLayoutListChanged);
    connect(m_userInter, &UserInter::LayoutChanged, this, &NativeUser::currentKBLayoutChanged);
    connect(m_userInter, &UserInter::NoPasswdLoginChanged, this, &NativeUser::noPasswdLoginChanged);
    connect(m_userInter, &UserInter::Use24HourFormatChanged, this, &NativeUser::use24HourFormatChanged);

    m_userInter->userName();
    m_userInter->locale();
    m_userInter->passwordStatus();
    m_userInter->fullName();

    // intercept account dbus path uid
    m_uid = path.mid(QString(ACCOUNTS_DBUS_PREFIX).size()).toUInt();
    m_userName = userPwdName(m_uid);
    m_noPasswdGrp = checkUserIsNoPWGrp(this);

    configAccountInfo(DDESESSIONCC::CONFIG_FILE + m_userName);
    setPath(path);
}

void NativeUser::configAccountInfo(const QString &account_config)
{
    QSettings settings(account_config, QSettings::IniFormat);
    settings.beginGroup("User");

    m_avatar = settings.value("Icon").toString();
    m_greeterBackground = toLocalFile(settings.value("GreeterBackground").toString());
    m_locale = settings.value("Locale").toString();

    //QSettings中分号为结束符，分号后面的数据不会被读取，因此使用QFile直接读取桌面背景图片路径
    QStringList desktopBackgrounds = readDesktopBackgroundPath(account_config);

    int index = settings.value("Workspace").toInt();
    //刚安装的系统中返回的Workspace为0
    index = index <= 0 ? 1 : index;
    if (desktopBackgrounds.size() >= index && index > 0) {
        m_desktopBackground = toLocalFile(desktopBackgrounds.at(index - 1));
    } else {
        qDebug() << "configAccountInfo get error index:" << index << ", backgrounds:" << desktopBackgrounds;
    }
}

QStringList NativeUser::readDesktopBackgroundPath(const QString &path)
{
    QStringList desktopBackgrounds;
    QFile file(path);
    if (file.exists()) {
        if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
            QTextStream read(&file);
            while (!read.atEnd()) {
                QString line = read.readLine();
                if (line.contains("DesktopBackgrounds=", Qt::CaseInsensitive)) {
                    QString paths = line.section('=',1);
                    desktopBackgrounds << paths.split(";");
                }
            }

            file.close();
        }
    }

    return desktopBackgrounds;
}

void NativeUser::setCurrentLayout(const QString &layout)
{
    m_userInter->SetLayout(layout);
}

QString NativeUser::displayName() const
{
    return m_userName;
}

QString NativeUser::avatarPath() const
{
    m_userInter->iconFile();
    return m_avatar;
}

QString NativeUser::greeterBackgroundPath() const
{
    m_userInter->greeterBackground();
    return m_greeterBackground;
}

QString NativeUser::desktopBackgroundPath() const
{
    m_userInter->desktopBackgrounds();
    return m_desktopBackground;
}

QStringList NativeUser::kbLayoutList()
{
    return m_userInter->historyLayout();
}

QString NativeUser::currentKBLayout()
{
    return m_userInter->layout();
}

bool NativeUser::isNoPasswdGrp() const
{
    return m_noPasswdGrp;
}

bool NativeUser::isUserIsvalid() const
{
    //无效用户的时候m_userInter是有效的
    return m_userInter->isValid();
}

bool NativeUser::is24HourFormat() const
{
    if(!isUserIsvalid() || m_isServer)
        return true;

    return m_userInter->use24HourFormat();
}

ADDomainUser::ADDomainUser(uid_t uid, QObject *parent)
    : User(parent)
{
    m_uid = uid;
}

void ADDomainUser::setUserDisplayName(const QString &name)
{
    if (m_displayName == name) {
        return;
    }

    m_displayName = name;

    emit displayNameChanged(name);
}

void ADDomainUser::setUserName(const QString &name)
{
    if (m_userName == name) {
        return;
    }

    m_userName = name;
}

void ADDomainUser::setUserInter(UserInter *user_inter)
{
    if (m_userInter == user_inter) {
        return;
    }

    m_userInter = user_inter;
}

void ADDomainUser::setUid(uid_t uid)
{
    if (m_uid == uid) {
        return;
    }

    m_uid = uid;
}

void ADDomainUser::setIsServerUser(bool is_server)
{
    if (m_isServer == is_server) {
        return;
    }

    m_isServer = is_server;
}

QString ADDomainUser::displayName() const
{
    return m_displayName.isEmpty() ? m_userName : m_displayName;
}

QString ADDomainUser::avatarPath() const
{
    return QString(":/img/default_avatar.svg");
}

QString ADDomainUser::greeterBackgroundPath() const
{
    QFileInfo background_info(DEFAULT_BACKGROUND);
    return background_info.canonicalFilePath();
}

QString ADDomainUser::desktopBackgroundPath() const
{
    QFileInfo background_info(DEFAULT_BACKGROUND);
    return background_info.canonicalFilePath();
}
