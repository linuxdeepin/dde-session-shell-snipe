#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QObject>

#include "dbuslockservice.h"
#include "authinterface.h"
#include "dbuslogin1manager.h"
#include <com_deepin_daemon_authenticate.h>

using com::deepin::daemon::Authenticate;

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
    bool isConnectSync() {return m_greeter->connectSync();}
    void onDisplayErrorInfo(const QString &msg);

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

public slots:
    void checkAccountName(const QString &name);

private:
    QLightDM::Greeter *m_greeter;
    DBusLockService   *m_lockInter;
    bool               m_authenticating;
    QString            m_password;
};

#endif  // GREETERWORKEK_H
