/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef USERINFO_H
#define USERINFO_H

#include "constants.h"
#include "public_func.h"

#include <QObject>

#include <com_deepin_daemon_accounts_user.h>

using UserInter = com::deepin::daemon::accounts::User;

class User : public QObject
{
    Q_OBJECT
public:
    enum UserType {
        Native,
        ADDomain,
        Default
    };

    enum ExpiredStatus {
        ExpiredNormal,
        ExpiredSoon,
        ExpiredAlready
    };

    struct LimitsInfo {
        bool locked;        // 认证锁定状态 --- true: 锁定  false: 解锁
        uint maxTries;      // 最大重试次数
        uint numFailures;   // 失败次数，一直累加
        uint unlockSecs;    // 本次锁定总解锁时间（秒），不会随着时间推移减少
        QString unlockTime; // 解锁时间（本地时间）
    };

    explicit User(QObject *parent = nullptr);
    explicit User(const User &user);
    virtual ~User() override;

    bool operator==(const User &user) const;

    inline bool isAutomaticLogin() const { return m_isAutomaticLogin; }
    inline bool isPasswordValid() const { return m_isPasswordValid; }
    inline bool isLogin() const { return m_isLogin; }
    inline bool isNoPasswordLogin() const { return m_isNoPasswordLogin || !m_isPasswordValid; }
    virtual inline bool isUserValid() const { return false; }
    inline bool isUse24HourFormat() const { return m_isUse24HourFormat; }

    inline int expiredDayLeft() const { return m_expiredDayLeft; }
    inline int expiredStatus() const { return m_expiredStatus; }
    inline int shortDateFormat() const { return m_shortDateFormat; }
    inline int shortTimeFormat() const { return m_shortTimeFormat; }
    inline int weekdayFormat() const { return m_weekdayFormat; }

    virtual inline int type() const { return Default; }
    inline QMap<int, LimitsInfo> *limitsInfo() const { return m_limitsInfo; }
    inline LimitsInfo limitsInfo(const int type) const { return m_limitsInfo->value(type); }
    inline QString avatar() const { return m_avatar; }
    inline QString displayName() const { return m_fullName.isEmpty() ? m_name : m_fullName; }
    inline QString fullName() const { return m_fullName; }
    inline QString greeterBackground() const { return m_greeterBackground; }
    inline QString keyboardLayout() const { return m_keyboardLayout; }
    inline QString locale() const { return m_locale; }
    inline QString name() const { return m_name; }
    virtual inline QString path() const { return QString(); }
    inline QStringList desktopBackgrounds() const { return m_desktopBackgrounds; }
    inline QStringList keyboardLayoutList() const { return m_keyboardLayoutList; }
    inline uid_t uid() const { return m_uid; }

    void updateLimitsInfo(const QString &info);
    void updateLoginStatus(const bool isLogin);

    virtual void setKeyboardLayout(const QString &keyboard) { Q_UNUSED(keyboard) }
    virtual void updatePasswordExpiredInfo() { }

signals:
    void avatarChanged(const QString &);
    void autoLoginStatusChanged(const bool);
    void desktopBackgroundChanged(const QString &);
    void displayNameChanged(const QString &);
    void greeterBackgroundChanged(const QString &);
    void keyboardLayoutChanged(const QString &);
    void keyboardLayoutListChanged(const QStringList &);
    void limitsInfoChanged(const QMap<int, LimitsInfo> *);
    void localeChanged(const QString &locale);
    void loginStatusChanged(const bool);
    void noPasswordLoginChanged(const bool);
    void shortDateFormatChanged(const int);
    void shortTimeFormatChanged(const int);
    void weekdayFormatChanged(const int);
    void use24HourFormatChanged(const bool);

protected:
    bool checkUserIsNoPWGrp(const User *user) const;
    QString toLocalFile(const QString &path) const;
    QString userPwdName(const uid_t uid) const;

protected:
    bool m_isAutomaticLogin;             // 自动登录
    bool m_isLogin;                      // 登录状态
    bool m_isNoPasswordLogin;            // 无密码登录
    bool m_isPasswordValid;              // 用户是否设置密码
    bool m_isUse24HourFormat;            // 24小时制
    int m_expiredDayLeft;                // 密码过期剩余天数
    int m_expiredStatus;                 // 密码过期状态
    int m_shortDateFormat;               // 短日期格式
    int m_shortTimeFormat;               // 短时间格式
    int m_weekdayFormat;                 // 星期显示格式
    uid_t m_uid;                         // 用户 uid
    QString m_avatar;                    // 用户头像
    QString m_fullName;                  // 用户全名
    QString m_greeterBackground;         // 登录/锁屏界面背景
    QString m_keyboardLayout;            // 键盘布局
    QString m_locale;                    // 语言环境
    QString m_name;                      // 用户名
    QStringList m_desktopBackgrounds;    // 桌面背景（不同工作区壁纸不同，故是个 List）
    QStringList m_keyboardLayoutList;    // 键盘布局列表
    QMap<int, LimitsInfo> *m_limitsInfo; // 认证限制信息
};

class NativeUser : public User
{
    Q_OBJECT
public:
    explicit NativeUser(const QString &path, QObject *parent = nullptr);
    explicit NativeUser(const uid_t &uid, QObject *parent = nullptr);
    explicit NativeUser(const NativeUser &user);

    inline bool isUserValid() const override { return m_userInter->isValid(); }

    inline int type() const override { return Native; }
    inline QString path() const override { return m_path; }

    void setKeyboardLayout(const QString &keyboard) override;

    void updatePasswordExpiredInfo() override;

private slots:
    void updateAvatar(const QString &path);
    void updateAutomaticLogin(const bool autoLoginState);
    void updateDesktopBackgrounds(const QStringList &backgrounds);
    void updateFullName(const QString &fullName);
    void updateGreeterBackground(const QString &path);
    void updateKeyboardLayout(const QString &keyboardLayout);
    void updateKeyboardLayoutList(const QStringList &keyboardLayoutList);
    void updateLocale(const QString &locale);
    void updateName(const QString &name);
    void updateNoPasswordLogin(const bool isNoPasswordLogin);
    void updatePasswordStatus(const QString &status);
    void updateShortDateFormat(const int format);
    void updateShortTimeFormat(const int format);
    void updateWeekdayFormat(const int format);
    void updateUid(const QString &uid);
    void updateUse24HourFormat(const bool is24HourFormat);

private:
    void initConnections();
    void initData();
    void initConfiguration(const QString &config);
    QStringList readDesktopBackgroundPath(const QString &path);

private:
    QString m_path;
    UserInter *m_userInter;
};

class ADDomainUser : public User
{
    Q_OBJECT
public:
    ADDomainUser(uid_t uid, QObject *parent = nullptr);

    inline int type() const override { return ADDomain; }
    void setFullName(const QString &fullName);
    void setName(const QString &name);
};

#endif // USERINFO_H
