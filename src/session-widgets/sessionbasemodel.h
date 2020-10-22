#ifndef SESSIONBASEMODEL_H
#define SESSIONBASEMODEL_H

#include "userinfo.h"

#include <QObject>
#include <memory>
#include <com_deepin_sessionmanager.h>

class SessionBaseModel : public QObject
{
    Q_OBJECT
public:
    enum AuthType {
        UnknowAuthType,
        LockType,
        LightdmType
    };

    enum AuthFaildType {
        Fprint,
        KEYBOARD
    };

    enum PowerAction {
        None,
        RequireNormal,
        RequireShutdown,
        RequireRestart,
        RequireSuspend,
        RequireHibernate,
    };

    enum ModeStatus {
        NoStatus,
        PasswordMode,
        ChangePasswordMode,
        ConfirmPasswordMode,
        UserMode,
        SessionMode,
        PowerMode
    };

    explicit SessionBaseModel(AuthType type, QObject *parent = nullptr);

    inline AuthType currentType() const { return m_currentType; }
    inline std::shared_ptr<User> currentUser() { return m_currentUser; }
    inline std::shared_ptr<User> lastLogoutUser() const { return m_lastLogoutUser; }

    std::shared_ptr<User> findUserByUid(const uint uid) const;
    std::shared_ptr<User> findUserByName(const QString &name) const;
    const QList<std::shared_ptr<User>> userList() const { return m_userList; }
    const QList<std::shared_ptr<User>> logindUser();

    void userAdd(std::shared_ptr<User> user);
    void userRemoved(std::shared_ptr<User> user);
    void setCurrentUser(std::shared_ptr<User> user);
    void setLastLogoutUser(const std::shared_ptr<User> &lastLogoutUser);

    inline QString sessionKey() const { return m_sessionKey; }
    void setSessionKey(const QString &sessionKey);

    inline PowerAction powerAction() const { return m_powerAction; }
    void setPowerAction(const PowerAction &powerAction);

    ModeStatus currentModeState() const { return m_currentModeState; }
    void setCurrentModeState(const ModeStatus &currentModeState);

    void setHasVirtualKB(bool hasVirtualKB);

    void setHasSwap(bool hasSwap);
    inline bool hasSwap() { return m_hasSwap; }

    inline bool isShow() const { return m_isShow; }
    void setIsShow(bool isShow);

    inline bool canSleep() const { return m_canSleep; }
    void setCanSleep(bool canSleep);

    inline bool allowShowUserSwitchButton() const { return m_allowShowUserSwitchButton; }
    void setAllowShowUserSwitchButton(bool allowShowUserSwitchButton);

    inline bool alwaysShowUserSwitchButton() const { return m_alwaysShowUserSwitchButton; }
    void setAlwaysShowUserSwitchButton(bool alwaysShowUserSwitchButton);

    inline bool isServerModel() const { return m_isServerModel; }
    void setIsServerModel(bool server_model);

    void setAbortConfirm(bool abortConfirm);
    void setLocked(bool lock);
    bool isLocked();

    inline bool isLockNoPassword() const { return m_isLockNoPassword; }
    void setIsLockNoPassword(bool LockNoPassword);

    inline bool isBlackMode() const { return m_isBlackMode; }
    void setIsBlackModel(bool is_black);

    inline bool isHibernateMode() const {return m_isHibernateMode; }
    void setIsHibernateModel(bool is_Hibernate);

signals:
    void onUserAdded(std::shared_ptr<User> user);
    void onUserRemoved(const uint uid);
    void currentUserChanged(std::shared_ptr<User> user);
    void authTipsMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFaildMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFaildTipsMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFinished(bool success);
    void switchUserFinished();
    void onPowerActionChanged(PowerAction poweraction);
    void onSessionKeyChanged(const QString &sessionKey);
    void onLogindUserChanged();
    void showUserList();
    void visibleChanged(bool visible);
    void onStatusChanged(ModeStatus status);
    void onUserListChanged(QList<std::shared_ptr<User>> list);
    void hasVirtualKBChanged(bool hasVirtualKB);
    void onHasSwapChanged(bool hasSwap);
    void canSleepChanged(bool canSleep);
    void allowShowUserSwitchButtonChanged(bool allowShowUserSwitchButton);
    void abortConfirmChanged(bool abortConfirm);
    void lockChanged(bool lock);
    void userListLoginedChanged(QList<std::shared_ptr<User>> list);
    void activeAuthChanged(bool active);
    void blackModeChanged(bool is_black);
    void HibernateModeChanged(bool is_hibernate);//休眠信号改变

private:
    com::deepin::SessionManager *m_sessionManagerInter;
    bool m_hasSwap;
    bool m_isShow;
    bool m_isServerModel;
    bool m_canSleep;
    bool m_allowShowUserSwitchButton;
    bool m_alwaysShowUserSwitchButton;
    bool m_abortConfirm;
    bool m_isLockNoPassword;
    bool m_isBlackMode;
    bool m_isHibernateMode;
    AuthType m_currentType;
    QList<std::shared_ptr<User>> m_userList;
    std::shared_ptr<User> m_currentUser;
    std::shared_ptr<User> m_lastLogoutUser;
    QString m_sessionKey;
    PowerAction m_powerAction;
    ModeStatus m_currentModeState;
};

#endif // SESSIONBASEMODEL_H
