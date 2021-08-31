#ifndef SESSIONBASEMODEL_H
#define SESSIONBASEMODEL_H

#include "userinfo.h"

#include <types/mfainfolist.h>

#include <QObject>
#include <memory>

#include <DSysInfo>
class SessionBaseModel : public QObject
{
    Q_OBJECT
public:
    enum AuthType {
        UnknowAuthType,
        LockType,
        LightdmType
    };
    Q_ENUM(AuthType)

    enum AuthFaildType {
        Fprint,
        KEYBOARD
    };
    Q_ENUM(AuthFaildType)

    enum PowerAction {
        None,
        RequireNormal,
        RequireShutdown,
        RequireRestart,
        RequireSuspend,
        RequireHibernate,
        RequireLock,
        RequireLogout,
        RequireSwitchUser,
        RequireSwitchSystem
    };
    Q_ENUM(PowerAction)

    enum ModeStatus {
        NoStatus,
        PasswordMode,
        ConfirmPasswordMode,
        UserMode,
        SessionMode,
        PowerMode,
        ShutDownMode
    };
    Q_ENUM(ModeStatus)

    struct AuthProperty {
        /* com.deepin.daemon.Authenticate */
        int FrameworkState;     // 认证框架是否可用标志位
        int MixAuthFlags;       // 受支持的认证类型
        QString EncryptionType; // 加密类型
        /* com.deepin.daemon.Authenticate.Session */
        int AuthType = 0; // 账户开启的认证类型
        int PINLen = 0;   // PIN 码的最大长度
        bool MFAFlag;     // 多因子标志位
        bool FuzzyMFA;    // ???
        QString Prompt;   // 提示语
        QString UserName; // 账户名
    };

    explicit SessionBaseModel(AuthType type, QObject *parent = nullptr);
    ~SessionBaseModel();

    inline AuthType currentType() const { return m_currentType; }

    inline std::shared_ptr<User> currentUser() const { return m_currentUser; }
    inline std::shared_ptr<User> lastLogoutUser() const { return m_lastLogoutUser; }

    inline QList<std::shared_ptr<User>> loginedUserList() const { return m_loginedUsers->values(); }
    inline QList<std::shared_ptr<User>> userList() const { return m_users->values(); }

    std::shared_ptr<User> findUserByUid(const uint uid) const;
    std::shared_ptr<User> findUserByName(const QString &name) const;

    inline QString sessionKey() const { return m_sessionKey; }
    void setSessionKey(const QString &sessionKey);

    inline PowerAction powerAction() const { return m_powerAction; }
    void setPowerAction(const PowerAction &powerAction);

    ModeStatus currentModeState() const { return m_currentModeState; }
    void setCurrentModeState(const ModeStatus &currentModeState);

    inline int userListSize() { return m_userListSize; }
    void setUserListSize(int users_size);

    void setHasVirtualKB(bool hasVirtualKB);

    void setHasSwap(bool hasSwap);
    inline bool hasSwap() { return m_hasSwap; }

    inline bool visible() const { return m_visible; }
    void setVisible(const bool visible);

    inline bool canSleep() const { return m_canSleep; }
    void setCanSleep(bool canSleep);

    inline bool allowShowUserSwitchButton() const { return m_allowShowUserSwitchButton; }
    void setAllowShowUserSwitchButton(bool allowShowUserSwitchButton);

    inline bool alwaysShowUserSwitchButton() const { return m_alwaysShowUserSwitchButton; }
    void setAlwaysShowUserSwitchButton(bool alwaysShowUserSwitchButton);

    inline bool isServerModel() const { return m_isServerModel; }
    void setIsServerModel(bool server_model);

    inline bool abortConfim() const { return m_abortConfirm; }
    void setAbortConfirm(bool abortConfirm);

    inline bool isLockNoPassword() const { return m_isLockNoPassword; }
    void setIsLockNoPassword(bool LockNoPassword);

    inline bool isBlackMode() const { return m_isBlackMode; }
    void setIsBlackModel(bool is_black);

    inline bool isHibernateMode() const { return m_isHibernateMode; }
    void setIsHibernateModel(bool is_Hibernate);

    inline bool isCheckedInhibit() const { return m_isCheckedInhibit; }
    void setIsCheckedInhibit(bool checked);

    inline bool allowShowCustomUser() const { return m_allowShowCustomUser; }
    void setAllowShowCustomUser(const bool allowShowCustomUser);

    inline const QList<std::shared_ptr<User>> getUserList() const { return m_users->values(); }

    inline const AuthProperty &getAuthProperty() const { return m_authProperty; }
    void setAuthType(const int type);

signals:
    /* com.deepin.daemon.Accounts */
    void currentUserChanged(const std::shared_ptr<User>);
    void userAdded(const std::shared_ptr<User>);
    void userRemoved(const std::shared_ptr<User>);
    void userListChanged(const QList<std::shared_ptr<User>>);
    void loginedUserListChanged(const QList<std::shared_ptr<User>>);
    /* others */
    void visibleChanged(const bool);

public slots:
    /* com.deepin.daemon.Accounts */
    void addUser(const QString &path);
    void addUser(const std::shared_ptr<User> user);
    void removeUser(const QString &path);
    void removeUser(const std::shared_ptr<User> user);
    void updateCurrentUser(const QString &userJson);
    void updateCurrentUser(const std::shared_ptr<User> user);
    void updateUserList(const QStringList &list);
    void updateLastLogoutUser(const uid_t uid);
    void updateLastLogoutUser(const std::shared_ptr<User> lastLogoutUser);
    void updateLoginedUserList(const QString &list);
    /* com.deepin.daemon.Authenticate */
    void updateLimitedInfo(const QString &info);
    void updateFrameworkState(const int state);
    void updateSupportedMixAuthFlags(const int flags);
    void updateSupportedEncryptionType(const QString &type);
    /* com.deepin.daemon.Authenticate.Session */
    void updateAuthStatus(const int type, const int status, const QString &message);
    void updateFactorsInfo(const MFAInfoList &info);
    void updateFuzzyMFA(const bool fuzzMFA);
    void updateMFAFlag(const bool MFAFlag);
    void updatePINLen(const int PINLen);
    void updatePrompt(const QString &prompt);

signals:
    void authTipsMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFaildMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFaildTipsMessage(const QString &message, AuthFaildType type = KEYBOARD);
    void authFinished(bool success);
    void switchUserFinished();
    void onPowerActionChanged(PowerAction poweraction);
    void onRequirePowerAction(PowerAction poweraction, bool needConfirm);
    void onSessionKeyChanged(const QString &sessionKey);
    void showUserList();
    void showLockScreen();
    void showShutdown();
    void onStatusChanged(ModeStatus status);
    void onUserListChanged(QList<std::shared_ptr<User>> list);
    void hasVirtualKBChanged(bool hasVirtualKB);
    void onUserListSizeChanged(int users_size);
    void onHasSwapChanged(bool hasSwap);
    void canSleepChanged(bool canSleep);
    void allowShowUserSwitchButtonChanged(bool allowShowUserSwitchButton);
    void abortConfirmChanged(bool abortConfirm);
    void lockLimitFinished();
    void userListLoginedChanged(QList<std::shared_ptr<User>> list);
    void activeAuthChanged(bool active);
    void blackModeChanged(bool is_black);
    void HibernateModeChanged(bool is_hibernate); //休眠信号改变
    void prepareForSleep(bool is_Sleep);          //待机信号改变
    void shutdownInhibit(const SessionBaseModel::PowerAction action, bool needConfirm);
    void cancelShutdownInhibit();
    void tipsShowed();
    void clearServerLoginWidgetContent();

    void authStatusChanged(const int, const int, const QString &);
    void authTypeChanged(const int type);

private:
    bool m_hasSwap;
    bool m_visible = false;
    bool m_isServerModel;
    bool m_canSleep;
    bool m_allowShowUserSwitchButton;
    bool m_alwaysShowUserSwitchButton;
    bool m_abortConfirm;
    bool m_isLockNoPassword;
    bool m_isBlackMode;
    bool m_isHibernateMode;
    bool m_isLock = false;
    bool m_allowShowCustomUser;
    int m_userListSize = 0;
    AuthType m_currentType;
    QList<std::shared_ptr<User>> m_userList;
    std::shared_ptr<User> m_currentUser;
    std::shared_ptr<User> m_lastLogoutUser;
    QString m_sessionKey;
    PowerAction m_powerAction;
    ModeStatus m_currentModeState;
    bool m_isCheckedInhibit = false;
    AuthProperty m_authProperty; // 认证相关属性的值，初始时通过dbus获取，暂存在model中，供widget初始化界面使用
    QMap<QString, std::shared_ptr<User>> *m_users;
    QMap<QString, std::shared_ptr<User>> *m_loginedUsers;
};

#endif // SESSIONBASEMODEL_H
