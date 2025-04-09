// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHINTERFACE_H
#define AUTHINTERFACE_H

#include "public_func.h"
#include "constants.h"
#include "dbuslogin1manager.h"

#ifndef ENABLE_DSS_SNIPE
#include <com_deepin_daemon_accounts.h>
#include <com_deepin_daemon_logined.h>
#include <com_deepin_daemon_authenticate.h>
#include <org_freedesktop_login1_session_self.h>
#include <com_deepin_daemon_powermanager.h>
#include <org_freedesktop_dbus.h>
#include <QGSettings>
#else
#include "authenticate1interface.h"
#include "accounts1interface.h"
#include "loginedinterface.h"
#include "powermanager1interface.h"
#include "dbusinterface.h"
#include "selfinterface.h"
#include <DConfig>
#endif

#include <QObject>
#include <memory>

#ifndef ENABLE_DSS_SNIPE
using AccountsInter = com::deepin::daemon::Accounts;
using LoginedInter = com::deepin::daemon::Logined;
using Login1SessionSelf = org::freedesktop::login1::Session;
using PowerManagerInter = com::deepin::daemon::PowerManager;
using DBusObjectInter = org::freedesktop::DBus;

using com::deepin::daemon::Authenticate;
#else
using AccountsInter = org::deepin::dde::Accounts1;
using LoginedInter = org::deepin::dde::Logined;
using Login1SessionSelf = org::freedesktop::login1::Session;
using PowerManagerInter = org::deepin::dde::PowerManager1;
using DBusObjectInter = org::freedesktop::DBus;

using Authenticate = org::deepin::dde::Authenticate1;
#endif

class User;
class SessionBaseModel;

namespace Auth {
class AuthInterface : public QObject {
    Q_OBJECT
public:
    explicit AuthInterface(SessionBaseModel *const model, QObject *parent = nullptr);

    virtual void switchToUser(std::shared_ptr<User> user) = 0;

    virtual void setKeyboardLayout(std::shared_ptr<User> user, const QString &layout);
    virtual void onUserListChanged(const QStringList &list);
    virtual void onUserAdded(const QString &user);
    virtual void onUserRemove(const QString &user);

    enum SwitchUser {
        Always = 0,
        Ondemand,
        Disabled
    };

protected:
    void initDBus();
    void initData();
    void onLoginUserListChanged(const QString &list);

    bool checkHaveDisplay(const QJsonArray &array);
    bool isLogined(uint uid);
    void checkConfig();
    void checkPowerInfo();
    bool checkIsADDomain();
    bool isDeepin();
#ifndef ENABLE_DSS_SNIPE
    QVariant getGSettings(const QString& node, const QString& key);
#else
    QVariant getDconfigValue(const QString &key, const QVariant &fallbackValue);
#endif
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
    DBusObjectInter*   m_dbusInter;
#ifndef ENABLE_DSS_SNIPE
    QGSettings*        m_gsettings = nullptr;
#else
    Dtk::Core::DConfig* m_dConfig = nullptr;
#endif
    uint               m_lastLogoutUid;
    uint               m_currentUserUid;
    std::list<uint>    m_loginUserList;
};
}  // namespace Auth

#endif  // AUTHINTERFACE_H
