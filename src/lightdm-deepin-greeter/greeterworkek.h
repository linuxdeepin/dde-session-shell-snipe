#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QObject>

#include "src/global_util/dbus/dbuslockservice.h"
#include "src/session-widgets/authinterface.h"
#include "src/global_util/dbus/dbuslogin1manager.h"
#include <com_deepin_daemon_authenticate.h>
#include <linux/version.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

using com::deepin::daemon::Authenticate;

class FileIOThread : public QThread {
    Q_OBJECT

public:
    void run();

Q_SIGNALS:
    void runShutDownProcess();

private:
    int print_events(int fd);
};

class GreeterWorkek : public Auth::AuthInterface
{
    Q_OBJECT
public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    explicit GreeterWorkek(SessionBaseModel *const model, QObject *parent = nullptr);

    void switchToUser(std::shared_ptr<User> user) override;
    void authUser(const QString &password) override;
    void onUserAdded(const QString &user) override;

signals:
    void requestUpdateBackground(const QString &path);

private:
    void checkDBusServer(bool isvalid);
    void oneKeyLogin();
    void onCurrentUserChanged(const QString &user);
    void userAuthForLightdm(std::shared_ptr<User> user);
    void prompt(QString text, QLightDM::Greeter::PromptType type);
    void message(QString text, QLightDM::Greeter::MessageType type);
    void authenticationComplete();
    void saveNumlockStatus(std::shared_ptr<User> user, const bool &on);
    void recoveryUserKBState(std::shared_ptr<User> user);
    void resetLightdmAuth(std::shared_ptr<User> user,int delay_time, bool is_respond);

    // 通过dde_wldpms命令控制屏幕开关
    void screenSwitchByWldpms(bool active);


    //onekeylogin && logout 调用
    void callAuthForLightdm(const QString &user) ;
private:
    QLightDM::Greeter *m_greeter;
    DBusLockService   *m_lockInter;
    Authenticate      *m_AuthenticateInter;
    bool               m_isThumbAuth;
    bool               m_authSuccess;
    bool               m_authenticating;
    bool               m_showAuthResult;
    bool               m_firstTimeLogin;
    QString            m_password;
    FileIOThread *m_monitorHisiThread;
};

#endif  // GREETERWORKEK_H
