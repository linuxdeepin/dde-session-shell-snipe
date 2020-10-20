#include "sessionbasemodel.h"

#include <QDebug>
#include <DSysInfo>

#define SessionManagerService "com.deepin.SessionManager"
#define SessionManagerPath "/com/deepin/SessionManager"

using namespace com::deepin;
DCORE_USE_NAMESPACE

SessionBaseModel::SessionBaseModel(AuthType type, QObject *parent)
    : QObject(parent)
    , m_sessionManagerInter(nullptr)
    , m_hasSwap(false)
    , m_isShow(false)
    , m_isServerModel(false)
    , m_canSleep(false)
    , m_isLockNoPassword(false)
    , m_currentType(type)
    , m_currentUser(nullptr)
    , m_powerAction(PowerAction::RequireNormal)
    , m_currentModeState(ModeStatus::NoStatus)
{
    if (m_currentType == LockType || m_currentType == UnknowAuthType) {
        m_sessionManagerInter = new SessionManager(SessionManagerService, SessionManagerPath, QDBusConnection::sessionBus(), this);
    }

    if ( m_currentType == UnknowAuthType ) {
        connect(m_sessionManagerInter,&SessionManager::LockedChanged,this,[this](bool locked) {
            if ( !locked )
                this->setIsShow(false);
        });
    }
}

std::shared_ptr<User> SessionBaseModel::findUserByUid(const uint uid) const
{
    for (auto user : m_userList) {
        if (user->uid() == uid) {
            return user;
        }
    }

    qDebug() << "Wrong, you shouldn't be here!";
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

    qDebug() << "Wrong, you shouldn't be here!";
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
    if (user_exist != nullptr && user_exist->metaObject() == &ADDomainUser::staticMetaObject) {
        userRemoved(user_exist);
    };

    m_userList << user;

    emit onUserAdded(user);
    emit onUserListChanged(m_userList);
}

void SessionBaseModel::userRemoved(std::shared_ptr<User> user)
{
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
    if (m_currentUser == user) return;

    m_currentUser = user;

    emit currentUserChanged(user);
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

void SessionBaseModel::setHasVirtualKB(bool hasVirtualKB)
{
    if (m_hasVirtualKB == hasVirtualKB) return;

    m_hasVirtualKB = hasVirtualKB;

    emit hasVirtualKBChanged(hasVirtualKB);
}

void SessionBaseModel::setHasSwap(bool hasSwap) {
    if (m_hasSwap == hasSwap) return;

    m_hasSwap = hasSwap;

    emit onHasSwapChanged(hasSwap);
}

void SessionBaseModel::setIsShow(bool isShow)
{
    if (m_isShow == isShow) return;

    m_isShow = isShow;

#ifndef QT_DEBUG

    if ( m_sessionManagerInter && m_currentType == UnknowAuthType ) {
        m_isShow = m_sessionManagerInter->locked() ? false : isShow ;
    }

    if (m_sessionManagerInter && m_currentType == LockType) {
        m_sessionManagerInter->SetLocked(m_isShow);
    }
#endif

    emit visibleChanged(m_isShow);

    //锁屏显示时，加载初始化屏幕键盘onboard进程，锁屏完成后结束onboard进程
    if (m_isShow) {
        setHasVirtualKB(QProcess::execute("which", QStringList() << "onboard") == 0);
    } else {
        setHasVirtualKB(false);
    }
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

void SessionBaseModel::setLocked(bool lock)
{
    if (m_sessionManagerInter) m_sessionManagerInter->SetLocked(lock);
}

bool SessionBaseModel::isLocked()
{
    return m_sessionManagerInter && m_sessionManagerInter->locked();
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
