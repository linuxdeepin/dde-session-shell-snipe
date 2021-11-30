#ifndef AUTHINTERFACE_H
#define AUTHINTERFACE_H

#include "src/global_util/public_func.h"
#include "src/global_util/constants.h"
#include "src/global_util/dbus/dbuslogin1manager.h"

#include <com_deepin_daemon_accounts.h>
#include <com_deepin_daemon_logined.h>
#include <com_deepin_daemon_authenticate.h>
#include <org_freedesktop_login1_session_self.h>
#include <com_deepin_daemon_powermanager.h>

#include <QJsonArray>
#include <QObject>
#include <QGSettings>
#include <memory>

using AccountsInter = com::deepin::daemon::Accounts;
using LoginedInter = com::deepin::daemon::Logined;
using Login1SessionSelf = org::freedesktop::login1::Session;
using com::deepin::daemon::Authenticate;
using PowerManagerInter = com::deepin::daemon::PowerManager;


class User;
class SessionBaseModel;

namespace Auth {
class AuthInterface : public QObject {
    Q_OBJECT
public:
    explicit AuthInterface(SessionBaseModel *const model, QObject *parent = nullptr);

    virtual void switchToUser(std::shared_ptr<User> user) = 0;
    virtual void authUser(const QString &password)        = 0;

    virtual void setLayout(std::shared_ptr<User> user, const QString &layout);
    virtual void onUserListChanged(const QStringList &list);
    virtual void onUserAdded(const QString &user);
    virtual void onUserRemove(const QString &user);
    void updateLockLimit(std::shared_ptr<User> user);

    enum SwitchUser {
        Always = 0,
        Ondemand,
        Disabled
    };

protected:
    void initDBus();
    void initData();
    void onLastLogoutUserChanged(uint uid);
    void onLoginUserListChanged(const QString &list);

    bool checkHaveDisplay(const QJsonArray &array);
    bool isLogined(uint uid);
    void checkConfig();
    void checkPowerInfo();
    void checkSwap();
    bool isDeepin();
    QVariant getGSettings(const QString& node, const QString& key);

    template <typename T>
    T valueByQSettings(const QString & group,
                       const QString & key,
                       const QVariant &failback) {
        return findValueByQSettings<T>(DDESESSIONCC::session_ui_configs,
                                       group,
                                       key,
                                       failback);
    }

protected:
    SessionBaseModel*  m_model;
    AccountsInter *    m_accountsInter;
    LoginedInter*      m_loginedInter;
    DBusLogin1Manager* m_login1Inter;
    Login1SessionSelf* m_login1SessionSelf = nullptr;
    PowerManagerInter* m_powerManagerInter;
    QGSettings*        m_gsettings = nullptr;
    uint               m_lastLogoutUid;
    uint               m_currentUserUid;
    std::list<uint>    m_loginUserList;
    Authenticate*      m_authenticateInter;

    QString            m_ukeyUserName;
    QString            m_ukeyUserInfo;
};
}  // namespace Auth

#endif  // AUTHINTERFACE_H
