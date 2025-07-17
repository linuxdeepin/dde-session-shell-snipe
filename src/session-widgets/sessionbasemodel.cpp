// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionbasemodel.h"
#include "dconfig_helper.h"
#include "dbus/dbusdisplaymanager.h"

#include <DSysInfo>
#include <QDebug>

#include "dbusconstant.h"

DCORE_USE_NAMESPACE

SessionBaseModel::SessionBaseModel(QObject *parent)
    : QObject(parent)
    , m_hasSwap(false)
    , m_visible(false)
    , m_isServerModel(false)
    , m_canSleep(false)
    , m_allowShowUserSwitchButton(false)
    , m_alwaysShowUserSwitchButton(false)
    , m_abortConfirm(false)
    , m_isBlackMode(false)
    , m_isHibernateMode(false)
    , m_allowShowCustomUser(false)
    , m_SEOpen(false)
    , m_isUseWayland(QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive))
    , m_appType(AuthCommon::None)
    , m_currentUser(nullptr)
    , m_powerAction(PowerAction::RequireNormal)
    , m_currentModeState(ModeStatus::NoStatus)
    , m_authProperty {false, false, Unavailable, AuthCommon::AT_None, AuthCommon::None, 0, "", "", ""}
    , m_users(new QMap<QString, std::shared_ptr<User>>())
    , m_loginedUsers(new QMap<QString, std::shared_ptr<User>>())
    , m_updatePowerMode(UPM_None)
    , m_currentContentType(NoContent)
    , m_lightdmPamStarted(false)
    , m_authResult{AuthType::AT_None, AuthState::AS_None, ""}
    , m_enableShellBlackMode(DConfigHelper::instance()->getConfig("enableShellBlack", true).toBool())
    , m_visibleShutdownWhenRebootOrShutdown(DConfigHelper::instance()->getConfig("visibleShutdownWhenRebootOrShutdown", true).toBool())
{
#ifndef ENABLE_DSS_SNIPE
    if (QGSettings::isSchemaInstalled("com.deepin.dde.power")) {
        m_powerGsettings = new QGSettings("com.deepin.dde.power", "/com/deepin/dde/power/", this);
    }
#endif
}

SessionBaseModel::~SessionBaseModel()
{
    delete m_users;
    delete m_loginedUsers;
}

#ifndef ENABLE_DSS_SNIPE
QVariant SessionBaseModel::getPowerGSettings(const QString &node, const QString &key)
{
    QVariant value = valueByQSettings<QVariant>(node, key, true);
    if (m_powerGsettings != nullptr && m_powerGsettings->keys().contains(key)) {
        value = m_powerGsettings->get(key);
    }
    return value;
}
#endif

std::shared_ptr<User> SessionBaseModel::findUserByUid(const uint uid) const
{
    for (auto user : qAsConst(*m_users)) {
        if (user->uid() == uid) {
            return user;
        }
    }
    return std::shared_ptr<User>(nullptr);
}

std::shared_ptr<User> SessionBaseModel::findUserByName(const QString &name) const
{
    if (name.isEmpty()) {
        return std::shared_ptr<User>(nullptr);
    }

    for (auto user : qAsConst(*m_users)) {
        if (user->name() == name) {
            return user;
        }
    }
    return std::shared_ptr<User>(nullptr);
}

std::shared_ptr<User> SessionBaseModel::findUserByPath(const QString &path) const
{
    return m_users->value(path, std::shared_ptr<User>(nullptr));
}

void SessionBaseModel::setAppType(const AppType type)
{
    m_appType = type;
}

void SessionBaseModel::setSessionKey(const QString &sessionKey)
{
    qCInfo(DDE_SHELL) << "Set session key:" << sessionKey;

    if (m_sessionKey == sessionKey)
        return;

    m_sessionKey = sessionKey;

    emit onSessionKeyChanged(sessionKey);
}

void SessionBaseModel::setPowerAction(const PowerAction &powerAction)
{
    qCInfo(DDE_SHELL) << "Incoming action:" << powerAction << ", current action:" << m_powerAction;

    if (powerAction == m_powerAction)
        return;

    m_powerAction = powerAction;

    emit onPowerActionChanged(powerAction);
}

void SessionBaseModel::setCurrentModeState(const ModeStatus &currentModeState)
{
    qCInfo(DDE_SHELL) << "Incoming state:" << currentModeState << ", current state:" << m_currentModeState;

    if (m_currentModeState == currentModeState)
        return;

    m_currentModeState = currentModeState;

    emit onStatusChanged(currentModeState);
}

void SessionBaseModel::setUserListSize(int users_size)
{
    if (m_userListSize == users_size)
        return;

    m_userListSize = users_size;

    emit onUserListSizeChanged(users_size);
}

void SessionBaseModel::setHasVirtualKB(bool hasVirtualKB)
{
    //锁屏显示时，加载初始化屏幕键盘onboard进程，锁屏完成后结束onboard进程
    if (hasVirtualKB && !qgetenv("XDG_SESSION_TYPE").contains("wayland")) {
        bool b = QProcess::execute("which", QStringList() << "onboard") == 0;
        emit hasVirtualKBChanged(b);
    } else {
        emit hasVirtualKBChanged(false);
    }
}

void SessionBaseModel::setHasSwap(bool hasSwap)
{
    qCInfo(DDE_SHELL) << "Has swap:" << hasSwap;
    if (m_hasSwap == hasSwap)
        return;

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
    qCInfo(DDE_SHELL) << "Set visible:" << visible << ", current visible:" << m_visible;

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
    qCInfo(DDE_SHELL) << "Can sleep:" << canSleep;
    if (m_canSleep == canSleep)
        return;

    m_canSleep = canSleep;

    emit canSleepChanged(canSleep);
}

void SessionBaseModel::setAllowShowUserSwitchButton(bool allowShowUserSwitchButton)
{
    if (m_allowShowUserSwitchButton == allowShowUserSwitchButton)
        return;

    m_allowShowUserSwitchButton = allowShowUserSwitchButton;

    emit allowShowUserSwitchButtonChanged(allowShowUserSwitchButton);
}

void SessionBaseModel::setAlwaysShowUserSwitchButton(bool alwaysShowUserSwitchButton)
{
    m_alwaysShowUserSwitchButton = alwaysShowUserSwitchButton;
}

void SessionBaseModel::setIsServerModel(bool server_model)
{
    if (m_isServerModel == server_model)
        return;

    m_isServerModel = server_model;
}

void SessionBaseModel::setAbortConfirm(bool abortConfirm)
{
    m_abortConfirm = abortConfirm;
    emit abortConfirmChanged(abortConfirm);
}

void SessionBaseModel::setIsBlackMode(bool is_black)
{
    if (!m_enableShellBlackMode) {
        return;
    }

    if (m_isBlackMode == is_black)
        return;

    m_isBlackMode = is_black;
    emit blackModeChanged(is_black);
}

void SessionBaseModel::setIsHibernateModel(bool is_Hibernate)
{
    if (m_isHibernateMode == is_Hibernate)
        return;

    m_isHibernateMode = is_Hibernate;
    emit HibernateModeChanged(is_Hibernate);
}

/**
 * @brief 显示自定义用户
 *
 * @param allowShowCustomUser
 */
void SessionBaseModel::setAllowShowCustomUser(const bool allowShowCustomUser)
{
    if (allowShowCustomUser == m_allowShowCustomUser)
        return;

    m_allowShowCustomUser = allowShowCustomUser;
}

/**
 * @brief 认证类型
 *
 * @param type
 */
void SessionBaseModel::setAuthType(const AuthFlags type)
{
    if (type == m_authProperty.AuthType && type != AT_None) {
        return;
    }
    if (m_currentUser->type() == User::Default) {
        m_authProperty.AuthType = type;
        emit authTypeChanged(AT_None);
    } else {
        m_authProperty.AuthType = type;
        emit authTypeChanged(type);
    }
}

/**
 * @brief 新增用户
 *
 * @param path 用户路径或 Uid
 */
void SessionBaseModel::addUser(const QString &path)
{
    qDebug("Add user, path: %s", qPrintable(path));

    if (m_users->contains(path)) {
        return;
    }
    std::shared_ptr<NativeUser> user(new NativeUser(path));
    m_users->insert(path, user);
    emit userAdded(user);
    emit userListChanged(m_users->values());
}

/**
 * @brief 新增用户
 *
 * @param user
 */
void SessionBaseModel::addUser(const std::shared_ptr<User> user)
{
    qInfo("Add user, path: %s, name: %s", qPrintable(user->path()), qPrintable(user->name()));

    const QList<std::shared_ptr<User>> userList = m_users->values();
    if (userList.contains(user)) {
        return;
    }
    const QString path = user->path().isEmpty() ? QString::number(user->uid()) : user->path();
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
    const std::shared_ptr<User> user = m_users->value(path);
    m_users->remove(path);
    emit userRemoved(user);
    emit userListChanged(m_users->values());
}

/**
 * @brief 删除用户
 *
 * @param user
 */
void SessionBaseModel::removeUser(const std::shared_ptr<User> user)
{
    qInfo("Remove user, name: %s, id: %d", qPrintable(user->name()), user->uid());

    const QList<std::shared_ptr<User>> userList = m_users->values();
    if (!userList.contains(user)) {
        return;
    }

    const QString path = user->path().isEmpty() ? QString::number(user->uid()) : user->path();
    m_users->remove(path);
    emit userRemoved(user);
}

/**
 * @brief json数据转成user对象
 *
 * @param userJson
 */
std::shared_ptr<User> SessionBaseModel::json2User(const QString &userJson)
{
    qDebug("user json data: %s", qPrintable(userJson));

    std::shared_ptr<User> user_ptr;
    QJsonParseError jsonParseError;
    const QJsonDocument userDoc = QJsonDocument::fromJson(userJson.toUtf8(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || userDoc.isEmpty()) {
        qWarning("Failed to obtain current user information from lock service!");
        return user_ptr;
    }

    const QJsonObject userObj = userDoc.object();
    const QString &name = userObj["Name"].toString();
    user_ptr = findUserByName(name);
    if (name.isEmpty() || user_ptr == nullptr) {
        const uid_t uid = static_cast<uid_t>(userObj["Uid"].toInt());
        user_ptr = findUserByUid(uid);
    }
    if (user_ptr) {
        user_ptr->setLastAuthType(AUTH_TYPE_CAST(userObj["AuthType"].toInt()));
        user_ptr->setLastCustomAuth(userObj["LastCustomAuth"].toString());
    }

    return user_ptr;
}

/**
 * @brief 保存当前用户信息
 *
 * @param userJson
 * @return 是否更新了用户
 */
bool SessionBaseModel::updateCurrentUser(const QString &userJson)
{
    qCInfo(DDE_SHELL) << "Update current user:" << userJson;
    std::shared_ptr<User> user_ptr = json2User(userJson);
    if (!user_ptr) {
        if (m_currentUser || m_users->isEmpty())
            return false;

        user_ptr = m_users->first();
    }

    return updateCurrentUser(user_ptr);
}

/**
 * @brief 保存当前用户信息
 *
 * @param user
 * @return 是否更新了用户
 */
bool SessionBaseModel::updateCurrentUser(const std::shared_ptr<User> user)
{
    if (!user) {
        qCWarning(DDE_SHELL) << "Failed to set current user!" << user.get();
        return false;
    }

    qCInfo(DDE_SHELL) << "Update current user:" << user->name();

    if (m_currentUser && *m_currentUser == *user) {
        qCWarning(DDE_SHELL) << "Same user to be updated" << user.get();
        m_currentUser->updateUserInfo();
        return false;
    }

    m_currentUser = user;
    emit currentUserChanged(user);

    return true;
}

/**
 * @brief 更新用户列表。当用户增加或减少时，会将新的用户列表通过信号发过来。
 *
 * @param list
 */
void SessionBaseModel::updateUserList(const QStringList &list)
{
    qCInfo(DDE_SHELL) << "Update user list: " << list;
    QStringList listTmp = m_users->keys();
    for (const QString &path : list) {
        if (m_users->contains(path)) {
            listTmp.removeAll(path);
        } else {
            std::shared_ptr<NativeUser> user(new NativeUser(path));
            m_users->insert(path, user);
        }
    }
    for (const QString &path : qAsConst(listTmp)) {
        m_users->remove(path);
    }
    emit userListChanged(m_users->values());
}

/**
 * @brief 更新已登录用户列表
 *
 * @param list
 */
void SessionBaseModel::updateLoginedUserList(const QString &list)
{
    qCDebug(DDE_SHELL) << "Logined user list: " << list;
    // kwin崩溃时systemd中的sessions不会被remove掉，这里通过DisplayManager的session信息过滤一遍（临时解决方案）
    QStringList loggedUserNameList;
    DBusDisplayManager displayManager("org.freedesktop.DisplayManager", "/org/freedesktop/DisplayManager", QDBusConnection::systemBus());
    const auto &sessions = displayManager.sessions();
    for (const auto &session : sessions) {
        const QString &sessionPath = session.path();
        if (sessionPath.isEmpty())
            continue;
        QDBusInterface interface("org.freedesktop.DisplayManager", sessionPath, "org.freedesktop.DisplayManager.Session", QDBusConnection::systemBus());
        if (interface.isValid()) {
            const QString &userName = interface.property("UserName").toString();
            if (!userName.isEmpty())
                loggedUserNameList.append(userName);
        }
    }
    qInfo(DDE_SHELL) << "Logined users from display manager: " << loggedUserNameList;

    QList<QString> loginedUsersTmp = m_loginedUsers->keys();
    QJsonParseError jsonParseError;
    const QJsonDocument loginedUserListDoc = QJsonDocument::fromJson(list.toUtf8(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError) {
        qCWarning(DDE_SHELL) << "The logined user list is wrong!";
        return;
    }
    const QJsonObject loginedUserListDocObj = loginedUserListDoc.object();
    for (const QString &loginedUserListDocKey : loginedUserListDocObj.keys()) {
        const QJsonArray loginedUserListArr = loginedUserListDocObj.value(loginedUserListDocKey).toArray();
        for (const QJsonValue &loginedUserStr : loginedUserListArr) {
            const QJsonObject loginedUserListObj = loginedUserStr.toObject();
            const int uid = loginedUserListObj["Uid"].toInt();
            const QString &desktop = loginedUserListObj["Desktop"].toString();
            if ((uid != 0 && uid < 1000) || desktop.isEmpty()) {
                // 排除非正常登录用户
                continue;
            }
            const QString path = QString(DSS_DBUS::accountsUserPath).arg(QString::number(uid));
            if (!m_loginedUsers->contains(path)) {
                // 对于通过自定义窗口输入的账户(域账户), 此时账户还没添加进来，导致下面m_users->value(path)为空指针，调用会导致程序奔溃
                // 因此在登录时，对于新增的账户，调用addUser先将账户添加进来，然后再去更新对应账户的登录状态
                addUser(path);
                auto user = m_users->value(path);
                if (user->name().isEmpty() || loggedUserNameList.contains(user->name())) {
                    m_loginedUsers->insert(path, user);
                    user->updateLoginState(true);
                }
            } else {
                loginedUsersTmp.removeAll(path);
            }
        }
    }
    for (const QString &path : qAsConst(loginedUsersTmp)) {
        m_loginedUsers->remove(path);
        m_users->value(path)->updateLoginState(false);
    }

    qCInfo(DDE_SHELL) << "Logined users: " << m_loginedUsers->keys();
    emit loginedUserListChanged(m_loginedUsers->values());
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
    qCInfo(DDE_SHELL) << "Deepin authenticate framework state:" << state;

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
    if (flags == m_authProperty.MixAuthFlags)
        return;

    m_authProperty.MixAuthFlags = flags;
}

/**
 * @brief 支持的加密类型
 *
 * @param type
 */
void SessionBaseModel::updateSupportedEncryptionType(const QString &type)
{
    qCInfo(DDE_SHELL) << "Support encryption type:" << type;
    if (type == m_authProperty.EncryptionType)
        return;

    m_authProperty.EncryptionType = type;
}

/**
 * @brief 模糊多因子信息，供 PAM 使用，暂时用上
 *
 * @param fuzzMFA
 */
void SessionBaseModel::updateFuzzyMFA(const bool fuzzMFA)
{
    if (fuzzMFA == m_authProperty.FuzzyMFA)
        return;

    m_authProperty.FuzzyMFA = fuzzMFA;
}

/**
 * @brief 多因子标志
 *
 * @param MFAFlag true: 使用多因子  false: 使用单因子
 */
void SessionBaseModel::updateMFAFlag(const bool MFAFlag)
{
    if (MFAFlag == m_authProperty.MFAFlag)
        return;

    m_authProperty.MFAFlag = MFAFlag;
    emit MFAFlagChanged(MFAFlag);
}

/**
 * @brief PIN 最大长度
 * @param length
 */
void SessionBaseModel::updatePINLen(const int PINLen)
{
    if (PINLen == m_authProperty.PINLen)
        return;

    m_authProperty.PINLen = PINLen;
}

/**
 * @brief 总的提示语
 *
 * @param prompt 提示语
 */
void SessionBaseModel::updatePrompt(const QString &prompt)
{
    if (prompt == m_authProperty.Prompt)
        return;

    m_authProperty.Prompt = prompt;
}

/**
 * @brief 更新多因子信息
 *
 * @param info
 */
void SessionBaseModel::updateFactorsInfo(const MFAInfoList &infoList)
{
    qCInfo(DDE_SHELL) << "Factors info:" << infoList;
    m_authProperty.AuthType = AT_None;
    switch (m_authProperty.FrameworkState) {
    case Available:
        for (const MFAInfo &info : infoList) {
            m_authProperty.AuthType |= AUTH_FLAGS_CAST(info.AuthType);
        }
        emit authTypeChanged(m_authProperty.AuthType);
        break;
    default:
        m_authProperty.AuthType = AT_PAM;
        emit authTypeChanged(AT_PAM);
        break;
    }
}

/**
 * @brief 更新认证状态
 *
 * @param currentAuthType
 * @param state
 * @param result
 */
void SessionBaseModel::updateAuthState(const AuthType type, const AuthState state, const QString &message)
{
    m_authResult.authState = state;
    m_authResult.authType = type;
    m_authResult.authMessage = message;
    switch (m_authProperty.FrameworkState) {
    case Available:
        emit authStateChanged(type, state, message);
        break;
    default:
        emit authStateChanged(AT_PAM, state, message);
        break;
    }
}

bool SessionBaseModel::isLightdmPamStarted() const
{
    return m_lightdmPamStarted;
}

void SessionBaseModel::setLightdmPamStarted(bool lightdmPamStarted)
{
    if(m_lightdmPamStarted != lightdmPamStarted){
        m_lightdmPamStarted = lightdmPamStarted;
        if(lightdmPamStarted){
            Q_EMIT lightdmPamStartedChanged();
        }
    }
}

void SessionBaseModel::setTerminalLocked(bool locked)
{
    if (locked == m_isTerminalLocked)
        return;

    m_isTerminalLocked = locked;
    emit terminalLockedChanged(locked);
}

void SessionBaseModel::sendTerminalLockedSignal()
{
    emit terminalLockedChanged(m_isTerminalLocked);
}

void SessionBaseModel::setUserlistVisible(bool visible)
{
    if (visible == m_userlistVisible) {
        return;
    }
    m_userlistVisible = visible;
}

void SessionBaseModel::setQuickLoginProcess(bool val)
{
    m_isQuickLoginProcess = val;
}