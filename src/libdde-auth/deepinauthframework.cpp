#include "deepinauthframework.h"
#include "interface/deepinauthinterface.h"
#include "src/session-widgets/userinfo.h"

#include <QThread>
#include <QTimer>
#include <QVariant>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>

#define AUTHRNTICATESERVICE "com.deepin.daemon.Authenticate"

DeepinAuthFramework::DeepinAuthFramework(DeepinAuthInterface *inter, QObject *parent)
    : QObject(parent)
    , m_interface(inter)
    , m_authenticateInter(new AuthInter(AUTHRNTICATESERVICE, "/com/deepin/daemon/Authenticate", QDBusConnection::systemBus(), this))
{
    m_authagent = new AuthAgent(this);

    connect(m_authenticateInter, &AuthInter::UkeyUserData, this, &DeepinAuthFramework::UkeyUserData);
}

DeepinAuthFramework::~DeepinAuthFramework()
{
    if (!m_authagent.isNull()) {
        if (m_pamAuth != 0) {
            //先取消上次验证请求
            m_authagent->setCancelAuth(true);
            pthread_cancel(m_pamAuth);
            pthread_join(m_pamAuth, nullptr);
            m_pamAuth = 0;
        }

        delete m_authagent;
        m_authagent = nullptr;
    }
}

bool DeepinAuthFramework::GetUkeyUserData(QString &username, QString &userinfo)
{
    QDBusPendingReply<QString, QString> reply = m_authenticateInter->GetUkeyUserData();
    if (reply.isError()) {
        qWarning() << "GetUkeyUserData error:" << reply.error().message();
        return false;
    }
    username = reply.argumentAt(0).toString();
    userinfo = reply.argumentAt(1).toString();
    return true;
}

bool DeepinAuthFramework::isAuthenticate() const
{
    return m_pamAuth != 0;
}

int DeepinAuthFramework::GetAuthType()
{
    return m_authagent->GetAuthType();
}

void* DeepinAuthFramework::pamAuthWorker(void *arg)
{
    DeepinAuthFramework* deepin_auth = static_cast<DeepinAuthFramework*>(arg);
    if(deepin_auth != nullptr && deepin_auth->m_currentUser != nullptr) {
        //开始验证,并重置变量等待输入密码
        deepin_auth->m_authagent->setCancelAuth(false);
        deepin_auth->m_authagent->Authenticate(deepin_auth->m_currentUser->name());
    } else {
        qDebug() << "pam auth worker deepin framework is nullptr";
    }

    return nullptr;
}

void DeepinAuthFramework::Authenticate(std::shared_ptr<User> user)
{
    if (user->isLock() || user->name().isEmpty()) return;

    m_password.clear();

    if (m_pamAuth != 0) {
        qDebug() << Q_FUNC_INFO << "pam auth cancel" << m_authagent->thread()->loopLevel();
        //先取消上次验证请求
        m_authagent->setCancelAuth(true);
        //发送退出线程消息
        pthread_cancel(m_pamAuth);
        //等待线程退出
        pthread_join(m_pamAuth, nullptr);
        m_pamAuth = 0;
    }

    m_currentUser = user;

    //创建验证线程,等待输入密码
    int rc = pthread_create(&m_pamAuth, nullptr, &pamAuthWorker, this);
    if (rc != 0) {
        qDebug() << "failed to create the authentication thread: %s" << strerror(errno);
    }
}

void DeepinAuthFramework::Responsed(const QString &password)
{
    if(m_authagent.isNull() || m_pamAuth == 0) {
        qDebug() << "pam auth agent is not start";
        return;
    }

    m_password = password;
    if (m_currentUser->isNoPasswdGrp() || (!m_currentUser->isNoPasswdGrp() && !m_password.isEmpty())) {
        qDebug() << "pam responsed password";

        m_authagent->Responsed(password);
    }
}

const QString DeepinAuthFramework::RequestEchoOff(const QString &msg)
{
    Q_UNUSED(msg);

    return m_password;
}

const QString DeepinAuthFramework::RequestEchoOn(const QString &msg)
{
    return msg;
}

void DeepinAuthFramework::DisplayErrorMsg(const QString &msg)
{
    if (m_interface == nullptr) {
        return;
    }
    m_interface->onDisplayErrorMsg(msg);
}

void DeepinAuthFramework::DisplayTextInfo(const QString &msg)
{
    if (m_interface == nullptr) {
        return;
    }
    m_interface->onDisplayTextInfo(msg);
}

void DeepinAuthFramework::RespondResult(const QString &msg)
{
    if (m_interface == nullptr) {
        return;
    }
    m_interface->onPasswordResult(msg);
}
