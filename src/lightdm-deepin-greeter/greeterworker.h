#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include "authinterface.h"
#include "dbuslockservice.h"
#include "dbuslogin1manager.h"
#include "deepinauthframework.h"
#include "sessionbasemodel.h"
#include "greeter_display_wayland.h"

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QObject>

#include <com_deepin_api_soundthemeplayer.h>

using SoundThemePlayerInter = com::deepin::api::SoundThemePlayer;

class GreeterWorker : public Auth::AuthInterface
{
    Q_OBJECT
public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    explicit GreeterWorker(SessionBaseModel *const model, QObject *parent = nullptr);
    ~GreeterWorker() override;

    void switchToUser(std::shared_ptr<User> user) override;
    bool isSecurityEnhanceOpen();

signals:
    void requestUpdateBackground(const QString &path);
    void requestShowPrompt(const QString &prompt);
    void requestShowMessage(const QString &message);
    void showLoginWindow(bool isShow);

public slots:
    /* New authentication framework */
    void createAuthentication(const QString &account);
    void destoryAuthentication(const QString &account);
    void startAuthentication(const QString &account, const int authType);
    void endAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);

    void checkAccount(const QString &account);
    void restartResetSessionTimer();
    void onAuthFinished();

private slots:
    void onAuthStateChanged(const int type, const int state, const QString &message);
    void onReceiptChanged(bool state);
    void onCurrentUserChanged(const std::shared_ptr<User> &user);

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void doPowerAction(const SessionBaseModel::PowerAction action);
    void setCurrentUser(const std::shared_ptr<User> user);

    void checkDBusServer(bool isValid);
    void showPrompt(const QString &text, const QLightDM::Greeter::PromptType type);
    void showMessage(const QString &text, const QLightDM::Greeter::MessageType type);
    void authenticationComplete();
    void saveNumlockState(std::shared_ptr<User> user, bool on);
    int getNumLockState(const QString &userName);
    void recoveryUserKBState(std::shared_ptr<User> user);
    void startGreeterAuth(const QString &account = QString());
    void changePasswd();
    void screenSwitchByWldpms(bool active);

private:
    QLightDM::Greeter *m_greeter;
    DeepinAuthFramework *m_authFramework;
    DBusLockService *m_lockInter;
    SoundThemePlayerInter *m_soundPlayerInter;
#ifdef USE_DEEPIN_WAYLAND
    GreeterDisplayWayland *m_greeterDisplayWayland;
#endif
    QTimer *m_resetSessionTimer;
    QTimer *m_limitsUpdateTimer;
    QString m_account;
    QString m_password;
    bool m_retryAuth;
};

#endif  // GREETERWORKEK_H
