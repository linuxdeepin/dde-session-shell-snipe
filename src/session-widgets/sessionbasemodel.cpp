#include "authcommon.h"
#include "sessionbasemodel.h"

#include <QDebug>
#include <DSysInfo>
#include <QGSettings>

#define SessionManagerService "com.deepin.SessionManager"
#define SessionManagerPath "/com/deepin/SessionManager"

using namespace AuthCommon;
using namespace com::deepin;
DCORE_USE_NAMESPACE

SessionBaseModel::SessionBaseModel(AuthType type, QObject *parent)
    : QObject(parent)
    , m_hasSwap(false)
    , m_visible(false)
    , m_isServerModel(false)
    , m_canSleep(false)
    , m_isLockNoPassword(false)
    , m_currentType(type)
    , m_currentUser(nullptr)
    , m_powerAction(PowerAction::RequireNormal)
    , m_currentModeState(ModeStatus::NoStatus)
    , m_users(new QMap<QString, std::shared_ptr<User>>())
    , m_loginedUsers(new QMap<QString, std::shared_ptr<User>>())
{
}

SessionBaseModel::~SessionBaseModel()
{
    for (const QString &key : m_users->keys()) {
        m_users->remove(key);
    }
    delete m_users;

    for (const QString &key : m_loginedUsers->keys()) {
        m_loginedUsers->remove(key);
    }
    delete m_loginedUsers;
}

std::shared_ptr<User> SessionBaseModel::findUserByUid(const uint uid) const
{
    for (auto user : m_userList) {
        if (user->uid() == uid) {
            return user;
        }
    }

    // qDebug() << "Wrong, you shouldn't be here!";
    return std::shared_ptr<User>(nullptr);
}

std::shared_ptr<User> SessionBaseModel::findUserByName(const QString &name) const
{
    if (name.isEmpty()) return std::shared_ptr<User>(nullptr);

    for (auto user : m_userList) {
        if (user->name() == name) {
            return user;
        }
    }

    // qDebug() << "Wrong, you shouldn't be here!";
    return std::shared_ptr<User>(nullptr);
}

const QList<std::shared_ptr<User> > SessionBaseModel::logindUser()
{
    QList<std::shared_ptr<User>> userList;
    for (auto user : m_userList) {
        if (user->isLogin()) {
            userList << user;
        }
    }

    return userList;
}

void SessionBaseModel::userAdd(std::shared_ptr<User> user)
{
    // NOTE(zorowk): If there are duplicate uids, delete ADDomainUser first
    auto user_exist = findUserByUid(user->uid());
    if (user_exist != nullptr) {
        return;
    }

    m_userList << user;

    emit onUserAdded(user);
    emit onUserListChanged(m_userList);
}

void SessionBaseModel::userRemoved(std::shared_ptr<User> user)
{
    if(user == nullptr) return;

    emit onUserRemoved(user->uid());

    m_userList.removeOne(user);
    emit onUserListChanged(m_userList);

    // NOTE(justforlxz): If the current user is deleted, switch to the
    // first unlogin user. If it does not exist, switch to the first login user.
    if (user == m_currentUser) {
        QList<std::shared_ptr<User>> logindUserList;
        QList<std::shared_ptr<User>> unloginUserList;
        for (auto it = m_userList.cbegin(); it != m_userList.cend(); ++it) {
            if ((*it)->isLogin()) {
                logindUserList << (*it);
            }
            else {
                unloginUserList << (*it);
            }
        }

        if (unloginUserList.isEmpty()) {
            if (!logindUserList.isEmpty()) {
                setCurrentUser(logindUserList.first());
            }
        }
        else {
            setCurrentUser(unloginUserList.first());
        }
    }
}

void SessionBaseModel::setCurrentUser(std::shared_ptr<User> user)
{
    if (m_currentUser != nullptr && m_currentUser->uid() == user->uid()) {
        return;
    }
    m_currentUser = user;
    emit currentUserChanged(user);
}

/**
 * @brief 设置当前用户
 *
 * @param userJson
 */
void SessionBaseModel::setCurrentUser(const QString &userJson)
{
    const QJsonObject userObj = QJsonDocument::fromJson(userJson.toUtf8()).object();
    const QString name = userObj["Name"].toString();
    std::shared_ptr<User> user_ptr = findUserByName(name);
    if (user_ptr != nullptr) {
        setCurrentUser(user_ptr);
    }
}

void SessionBaseModel::setLastLogoutUser(const std::shared_ptr<User> &lastLogoutUser)
{
    m_lastLogoutUser = lastLogoutUser;
}

void SessionBaseModel::setSessionKey(const QString &sessionKey)
{
    if (m_sessionKey == sessionKey) return;

    m_sessionKey = sessionKey;

    emit onSessionKeyChanged(sessionKey);
}

void SessionBaseModel::setPowerAction(const PowerAction &powerAction)
{
    if (powerAction == m_powerAction) return;

    m_powerAction = powerAction;

    emit onPowerActionChanged(powerAction);
}

void SessionBaseModel::setCurrentModeState(const ModeStatus &currentModeState)
{
    if (m_currentModeState == currentModeState) return;

    m_currentModeState = currentModeState;

    emit onStatusChanged(currentModeState);
}

void SessionBaseModel::setUserListSize(int users_size)
{
    if(m_userListSize == users_size) return;

    m_userListSize = users_size;

    emit onUserListSizeChanged(users_size);
}

void SessionBaseModel::setHasVirtualKB(bool hasVirtualKB)
{
    //锁屏显示时，加载初始化屏幕键盘onboard进程，锁屏完成后结束onboard进程
    if (hasVirtualKB){
        bool b = QProcess::execute("which", QStringList() << "onboard") == 0;
        emit hasVirtualKBChanged(b);
    } else {
        emit hasVirtualKBChanged(false);
    }
}

void SessionBaseModel::setHasSwap(bool hasSwap) {
    if (m_hasSwap == hasSwap) return;

    m_hasSwap = hasSwap;

    emit onHasSwapChanged(hasSwap);
}

/**
 * @brief 设置界面显示状态
 *
 * @param visible
 */
void SessionBaseModel::setVisible(const bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;

    //根据界面显示还是隐藏设置是否加载虚拟键盘
    setHasVirtualKB(m_visible);

    emit visibleChanged(m_visible);
}

void SessionBaseModel::setCanSleep(bool canSleep)
{
    if (m_canSleep == canSleep) return;

    m_canSleep = canSleep;

    emit canSleepChanged(canSleep);
}

void SessionBaseModel::setAllowShowUserSwitchButton(bool allowShowUserSwitchButton)
{
    if (m_allowShowUserSwitchButton == allowShowUserSwitchButton) return;

    m_allowShowUserSwitchButton = allowShowUserSwitchButton;

    emit allowShowUserSwitchButtonChanged(allowShowUserSwitchButton);
}

void SessionBaseModel::setAlwaysShowUserSwitchButton(bool alwaysShowUserSwitchButton)
{
    m_alwaysShowUserSwitchButton = alwaysShowUserSwitchButton;
}

void SessionBaseModel::setIsServerModel(bool server_model)
{
    if (m_isServerModel == server_model) return;

    m_isServerModel = server_model;
}

void SessionBaseModel::setAbortConfirm(bool abortConfirm)
{
    m_abortConfirm = abortConfirm;
    emit abortConfirmChanged(abortConfirm);
}

void SessionBaseModel::setIsLockNoPassword(bool LockNoPassword)
{
   if (m_isLockNoPassword == LockNoPassword) return;

    m_isLockNoPassword = LockNoPassword;
}

void SessionBaseModel::setIsBlackModel(bool is_black)
{
    if(m_isBlackMode == is_black) return;

    m_isBlackMode = is_black;
    emit blackModeChanged(is_black);
}

void SessionBaseModel::setIsHibernateModel(bool is_Hibernate){
    if(m_isHibernateMode == is_Hibernate) return;
    m_isHibernateMode = is_Hibernate;
    emit HibernateModeChanged(is_Hibernate);
}

void SessionBaseModel::setIsCheckedInhibit(bool checked)
{
    if (m_isCheckedInhibit == checked) return;
    m_isCheckedInhibit = checked;
}

/**
 * @brief 显示自定义用户
 *
 * @param allowShowCustomUser
 */
void SessionBaseModel::setAllowShowCustomUser(const bool allowShowCustomUser)
{
    if (allowShowCustomUser == m_allowShowCustomUser) {
        return;
    }
    m_allowShowCustomUser = allowShowCustomUser;
}

/**
 * @brief 认证类型
 *
 * @param type
 */
void SessionBaseModel::setAuthType(const int type)
{
    m_authProperty.AuthType = type;
    emit authTypeChanged(type);
}

/**
 * @brief 新增用户
 *
 * @param path 用户路径或 Uid
 */
void SessionBaseModel::addUser(const QString &path)
{
    if (m_users->contains(path)) {
        return;
    }
    std::shared_ptr<User> user;
    uid_t uid = path.mid(QString(ACCOUNTS_DBUS_PREFIX).size()).toUInt();
    if (uid < 10000) {
        user = std::make_shared<NativeUser>(path);
    } else {
        user = std::make_shared<ADDomainUser>(static_cast<uid_t>(path.toInt()));
    }
    m_users->insert(path, user);
    emit userAdded(user);
}

/**
 * @brief 删除用户
 *
 * @param path 用户路径或 Uid
 */
void SessionBaseModel::removeUser(const QString &path)
{
    if (!m_users->contains(path)) {
        return;
    }
    m_users->remove(path);
    emit userRemoved(m_users->value(path));
}

/**
 * @brief 更新用户列表。当用户增加或减少时，会将新的用户列表通过信号发过来。
 *
 * @param list
 */
void SessionBaseModel::updateUserList(const QStringList &list)
{
    QStringList listTmp = m_users->keys();
    std::shared_ptr<User> user;
    for (const QString &path : list) {
        if (m_users->contains(path)) {
            listTmp.removeAll(path);
        } else {
            uid_t uid = path.mid(QString(ACCOUNTS_DBUS_PREFIX).size()).toUInt();
            if (uid < 10000) {
                user = std::make_shared<NativeUser>(path);
            } else {
                user = std::make_shared<ADDomainUser>(static_cast<uid_t>(path.toInt()));
            }
            m_users->insert(path, user);
        }
    }
    for (const QString &path : listTmp) {
        m_users->remove(path);
    }
    emit userListChanged(m_users->values());
}

/**
 * @brief 更新上一个登录用户的 uid
 *
 * @param id
 */
void SessionBaseModel::updateLastLogoutUser(const int uid)
{
    // TODO
}

/**
 * @brief 更新已登录用户列表
 *
 * @param list
 */
void SessionBaseModel::updateLoginedUserList(const QString &list)
{
    std::shared_ptr<User> user_ptr;
    QList<QString> loginedUsersTmp = m_loginedUsers->keys();
    const QJsonDocument loginedUserListDoc = QJsonDocument::fromJson(list.toUtf8());
    const QJsonArray loginedUserListArr = loginedUserListDoc.array();
    for (const QJsonValue &loginedUserStr : loginedUserListArr) {
        const QJsonObject loginedUserListObj = loginedUserStr.toObject();
        const int uid = loginedUserListObj["Uid"].toInt();
        const QString path = QString("/com/deepin/daemon/Accounts/User") + QString::number(uid);
        if (!m_loginedUsers->contains(QString::number(uid)) && !m_loginedUsers->contains(path)) {
            if (uid > 10000) {
                user_ptr = std::make_shared<ADDomainUser>(uid);
                m_loginedUsers->insert(QString::number(uid), user_ptr);
            } else {
                user_ptr = std::make_shared<NativeUser>(path);
                m_loginedUsers->insert(path, user_ptr);
            }
            user_ptr->setisLogind(true);
        } else if (m_loginedUsers->contains(QString::number(uid))) {
            loginedUsersTmp.removeAll(QString::number(uid));
        } else if (m_loginedUsers->contains(path)) {
            loginedUsersTmp.removeAll(path);
        }
    }
    for (const QString &path : loginedUsersTmp) {
        m_loginedUsers->remove(path);
    }
    /* 更新所有用户的登录状态 */
    for (const QString &path : m_loginedUsers->keys()) {
        if (m_users->contains(path)) {
            m_users->value(path)->setisLogind(true);
        } else {
            m_users->value(path)->setisLogind(false);
        }
    }
}

/**
 * @brief 更新账户限制信息
 *
 * @param info
 */
void SessionBaseModel::updateLimitedInfo(const QString &info)
{
    m_currentUser->updateLimitsInfo(info);
}

/**
 * @brief 认证框架类型
 *
 * @param state
 */
void SessionBaseModel::updateFrameworkState(const int state)
{
    if (state == m_authProperty.FrameworkState) {
        return;
    }
    m_authProperty.FrameworkState = state;
}

/**
 * @brief 支持的认证类型
 *
 * @param flags
 */
void SessionBaseModel::updateSupportedMixAuthFlags(const int flags)
{
    if (flags == m_authProperty.MixAuthFlags) {
        return;
    }
    m_authProperty.MixAuthFlags = flags;
}

/**
 * @brief 支持的加密类型
 *
 * @param type
 */
void SessionBaseModel::updateSupportedEncryptionType(const QString &type)
{
    if (type == m_authProperty.EncryptionType) {
        return;
    }
    m_authProperty.EncryptionType = type;
}

/**
 * @brief 模糊多因子信息，供 PAM 使用，暂时用上
 *
 * @param fuzzMFA
 */
void SessionBaseModel::updateFuzzyMFA(const bool fuzzMFA)
{
    if (fuzzMFA == m_authProperty.FuzzyMFA) {
        return;
    }
    m_authProperty.FuzzyMFA = fuzzMFA;
}

/**
 * @brief 多因子标志
 *
 * @param MFAFlag true: 使用多因子  false: 使用单因子
 */
void SessionBaseModel::updateMFAFlag(const bool MFAFlag)
{
    if (MFAFlag == m_authProperty.MFAFlag) {
        return;
    }
    m_authProperty.MFAFlag = MFAFlag;
}

/**
 * @brief PIN 最大长度
 * @param length
 */
void SessionBaseModel::updatePINLen(const int PINLen)
{
    if (PINLen == m_authProperty.PINLen) {
        return;
    }
    m_authProperty.PINLen = PINLen;
}

/**
 * @brief 总的提示语
 *
 * @param prompt 提示语
 */
void SessionBaseModel::updatePrompt(const QString &prompt)
{
    if (prompt == m_authProperty.Prompt) {
        return;
    }
    m_authProperty.Prompt = prompt;
}

/**
 * @brief 更新多因子信息
 *
 * @param info
 */
void SessionBaseModel::updateFactorsInfo(const MFAInfoList &infoList)
{
    m_authProperty.AuthType = AuthTypeNone;
    if (m_currentUser->uid() == INT_MAX) {
        emit authTypeChanged(AuthTypeNone);
        return;
    }
    switch (m_authProperty.FrameworkState) {
    case 0:
        if (m_authProperty.MFAFlag) {
            for (const MFAInfo &info : infoList) {
                m_authProperty.AuthType |= info.AuthType;
            }
            emit authTypeChanged(m_authProperty.AuthType);
        } else {
            m_authProperty.AuthType = AuthTypeSingle;
            emit authTypeChanged(AuthTypeSingle);
        }
        break;
    default:
        m_authProperty.AuthType = AuthTypeSingle;
        emit authTypeChanged(AuthTypeSingle);
        break;
    }
}

/**
 * @brief 更新认证状态
 *
 * @param currentAuthType
 * @param status
 * @param result
 */
void SessionBaseModel::updateAuthStatus(const int type, const int status, const QString &result)
{
    qInfo() << "Authentication Service status:" << type << status << result;
    switch (m_authProperty.FrameworkState) {
    case 0:
        if (m_authProperty.MFAFlag) {
            emit authStatusChanged(type, status, result);
        } else {
            emit authStatusChanged(AuthTypeSingle, status, result);
        }
        break;
    default:
        emit authStatusChanged(AuthTypeSingle, status, result);
        break;
    }
}
