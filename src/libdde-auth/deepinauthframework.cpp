#include "deepinauthframework.h"
#include "interface/deepinauthinterface.h"
#include "src/session-widgets/userinfo.h"

#include <QTimer>
#include <QVariant>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

static std::shared_ptr<User> USER;
static QString PASSWORD;

DeepinAuthFramework::DeepinAuthFramework(DeepinAuthInterface *inter, QObject *parent)
    : QObject(parent)
    , m_interface(inter)
    , m_type(UnknowAuthType)
{
}

DeepinAuthFramework::~DeepinAuthFramework()
{
}

bool DeepinAuthFramework::isAuthenticate() const
{
    return !m_keyboard.isNull() || !m_fprint.isNull();
}

void DeepinAuthFramework::SetUser(std::shared_ptr<User> user)
{
    USER = user;
}

void DeepinAuthFramework::keyBoardAuth()
{
    if (USER->isLock() || USER->uid() != m_currentUserUid) return;
    qDebug()<<QString("The KeyBoard Auth process is running ! The User id is : %1").arg(m_currentUserUid);

    if (m_keyboard == nullptr) {
        m_keyboard = new AuthAgent(AuthAgent::Keyboard, this);
        m_keyboard->SetUser(USER->name());

        qDebug()<<QString("User is NoPasswdGrp: %1 and Password is Empty: %2").arg(USER->isNoPasswdGrp()).arg(PASSWORD.isEmpty());
        if (USER->isNoPasswdGrp() || (!USER->isNoPasswdGrp() && !PASSWORD.isEmpty())) {
            qDebug() << Q_FUNC_INFO << "keyboard auth start";
            QTimer::singleShot(100, m_keyboard, &AuthAgent::Authenticate);
        }
    }
}

void DeepinAuthFramework::fprintAuth()
{
    if (USER->isLock() || USER->uid() != m_currentUserUid) return;

    if (m_fprint == nullptr) {
        m_fprint = new AuthAgent(AuthAgent::Fprint, this);
        m_fprint->SetUser(USER->name());
        // It takes time to auth again after cancel!
        qDebug() << Q_FUNC_INFO << "fprint auth start";
        QTimer::singleShot(500, m_fprint, &AuthAgent::Authenticate);
    }
}

void DeepinAuthFramework::Authenticate()
{
    fprintAuth();
    keyBoardAuth();
}

void DeepinAuthFramework::Clear()
{
    if (!m_keyboard.isNull()) {
        delete m_keyboard;
        m_keyboard = nullptr;
    }

    if (!m_fprint.isNull()) {
        delete m_fprint;
        m_fprint = nullptr;
    }

    PASSWORD.clear();
}

void DeepinAuthFramework::setPassword(const QString &password)
{
    PASSWORD = password;
}

void DeepinAuthFramework::setAuthType(DeepinAuthFramework::AuthType type)
{
    m_type = type;
}

void DeepinAuthFramework::setCurrentUid(uint uid)
{
    m_currentUserUid = uid;
}

const QString DeepinAuthFramework::RequestEchoOff(const QString &msg)
{
    Q_UNUSED(msg);

    return PASSWORD;
}

const QString DeepinAuthFramework::RequestEchoOn(const QString &msg)
{
    return msg;
}

void DeepinAuthFramework::DisplayErrorMsg(AuthAgent::Type type, const QString &errtype, const QString &msg)
{
    Q_UNUSED(type);

    m_interface->onDisplayErrorMsg(type, errtype, msg);
}

void DeepinAuthFramework::DisplayTextInfo(AuthAgent::Type type, const QString &msg)
{
    if (type == AuthAgent::Type::Fprint && !msg.isEmpty()) {
        m_interface->onDisplayTextInfo(type, tr("Verify your fingerprint or password"));
    } else {
        m_interface->onDisplayTextInfo(type, msg);
    }
}

void DeepinAuthFramework::RespondResult(AuthAgent::Type type, const QString &msg)
{
    m_interface->onPasswordResult(type, msg);
}
