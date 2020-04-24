#ifndef USERINFO_H
#define USERINFO_H

#include <QObject>
#include <com_deepin_daemon_accounts_user.h>

#include <memory>

#define ACCOUNT_DBUS_SERVICE "com.deepin.daemon.Accounts"
#define ACCOUNT_DBUS_PATH "/com/deepin/daemon/Accounts"

using UserInter = com::deepin::daemon::accounts::User;

class User : public QObject
{
    Q_OBJECT

public:
    enum UserType
    {
        Native,
        ADDomain,
    };

    User(QObject *parent);
    User(const User &user);

signals:
    void displayNameChanged(const QString &displayname) const;
    void logindChanged(bool islogind) const;
    void avatarChanged(const QString &avatar) const;
    void greeterBackgroundPathChanged(const QString &path) const;
    void desktopBackgroundPathChanged(const QString &path) const;
    void localeChanged(const QString &locale) const;
    void kbLayoutListChanged(const QStringList &list);
    void currentKBLayoutChanged(const QString &layout);
    void lockChanged(bool lock);

public:
    bool operator==(const User &user) const;
    const QString name() const { return m_userName; }

    bool isLogin() const { return m_isLogind; }
    uint uid() const { return m_uid; }

    const QString locale() const { return m_locale; }
    void setLocale(const QString &locale);

    virtual bool isNoPasswdGrp() const;
    virtual bool isPasswordExpired() const { return false; }
    virtual bool isUserIsvalid() const;
    virtual bool isDoMainUser() const { return m_isServer; }

    void setisLogind(bool isLogind);
    virtual void setCurrentLayout(const QString &layout) { Q_UNUSED(layout); }

    void setPath(const QString &path);
    const QString path() const { return m_path; }

    uint lockNum() const { return m_lockNum; }
    bool isLock() const { return m_isLock; }
    bool isLockForNum();
    void startLock();
    void resetLock();

    virtual UserType type() const = 0;
    virtual QString displayName() const { return m_userName; }
    virtual QString avatarPath() const = 0;
    virtual QString greeterBackgroundPath() const = 0;
    virtual QString desktopBackgroundPath() const = 0;
    virtual QStringList kbLayoutList() { return QStringList(); }
    virtual QString currentKBLayout() { return QString(); }
    void onLockTimeOut();

protected:
    bool m_isLogind;
    bool m_isLock;
    bool m_isServer = false;
    uint m_uid;
    uint m_lockNum; // minute basis
    uint m_tryNum; // try number
    uint m_lockMinutes;
    QString m_userName;
    QString m_fullName;
    QString m_locale;
    QString m_path;
    std::shared_ptr<QTimer> m_lockTimer;
    time_t m_startTime;//记录账号锁定的时间搓
};

class NativeUser : public User
{
    Q_OBJECT

public:
    NativeUser(const QString &path, QObject *parent = nullptr);
    UserInter *getUserInter() { return m_userInter; }

    void setCurrentLayout(const QString &currentKBLayout) override;
    UserType type() const override { return Native; }
    QString displayName() const override;
    QString avatarPath() const override;
    QString greeterBackgroundPath() const override;
    QString desktopBackgroundPath() const override;
    QStringList kbLayoutList() override;
    QString currentKBLayout() override;
    bool isNoPasswdGrp() const override;
    bool isPasswordExpired() const override;
    bool isUserIsvalid() const override;

private:
    UserInter *m_userInter;
};

class ADDomainUser : public User
{
    Q_OBJECT

public:
    ADDomainUser(uint uid, QObject *parent = nullptr);

    void setUserDisplayName(const QString &name);
    void setUserName(const QString &name);
    void setUserInter(UserInter *user_inter);
    void setUid(uint uid);
    void setIsServerUser(bool is_server);

    QString displayName() const override;
    UserType type() const override { return ADDomain; }
    QString avatarPath() const override;
    QString greeterBackgroundPath() const override;
    QString desktopBackgroundPath() const override;
    bool isPasswordExpired() const override;

private:
    QString m_displayName;
    UserInter *m_userInter = nullptr;
};

#endif // USERINFO_H
