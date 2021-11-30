#ifndef DEEPINAUTHFRAMEWORK_H
#define DEEPINAUTHFRAMEWORK_H

#include "authagent.h"
#include <com_deepin_daemon_authenticate.h>

#include <QObject>
#include <QPointer>
#include <memory>

using AuthInter = com::deepin::daemon::Authenticate;

class DeepinAuthInterface;
class QThread;
class User;
class DeepinAuthFramework : public QObject
{
    Q_OBJECT
public:
    explicit DeepinAuthFramework(DeepinAuthInterface *inter, QObject *parent = nullptr);
    ~DeepinAuthFramework();

    bool GetUkeyUserData(QString &username, QString &userinfo);

    friend class AuthAgent;
    bool isAuthenticate() const;
    int GetAuthType();

signals:
    /* com.deepin.daemon.Authenticate */
    void UkeyUserData(const QString &, const QString &);

public slots:
    void Authenticate(std::shared_ptr<User> user);
    void Responsed(const QString &password);

private:
    static void* pamAuthWorker(void *arg);
    const QString RequestEchoOff(const QString &msg);
    const QString RequestEchoOn(const QString &msg);
    void DisplayErrorMsg(const QString &msg);
    void DisplayTextInfo(const QString &msg);
    void RespondResult(const QString &msg);

private:
    AuthInter *m_authenticateInter;
    DeepinAuthInterface *m_interface;
    QPointer<AuthAgent> m_authagent;
    std::shared_ptr<User> m_currentUser = nullptr;
    QString m_password;
    pthread_t m_pamAuth = 0;
};

#endif // DEEPINAUTHFRAMEWORK_H
