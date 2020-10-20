#include "authagent.h"
#include "deepinauthframework.h"
#include "src/global_util/public_func.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <unistd.h>
#include <security/pam_appl.h>

#ifdef PAM_SUN_CODEBASE
#define PAM_MSG_MEMBER(msg, n, member) ((*(msg))[(n)].member)
#else
#define PAM_MSG_MEMBER(msg, n, member) ((msg)[(n)]->member)
#endif

#define PAM_SERVICE_SYSTEM_NAME "password-auth"
#define PAM_SERVICE_DEEPIN_NAME "common-auth"

AuthAgent::AuthAgent(DeepinAuthFramework *deepin)
    : m_deepinauth(deepin)
    , m_isCondition(true)
{
    connect(this, &AuthAgent::displayErrorMsg, deepin, &DeepinAuthFramework::DisplayErrorMsg, Qt::QueuedConnection);
    connect(this, &AuthAgent::displayTextInfo, deepin, &DeepinAuthFramework::DisplayTextInfo, Qt::QueuedConnection);
    connect(this, &AuthAgent::respondResult, deepin, &DeepinAuthFramework::RespondResult, Qt::QueuedConnection);
}

AuthAgent::~AuthAgent()
{
}

void AuthAgent::Responsed(const QString &password)
{
    m_password = password;
    m_isCondition = false;
}

void AuthAgent::Authenticate(const QString& username)
{
    pam_handle_t* m_pamHandle = nullptr;
    pam_conv conv = { pamConversation, static_cast<void*>(this) };
    const char* serviceName = isDeepinAuth() ? PAM_SERVICE_DEEPIN_NAME : PAM_SERVICE_SYSTEM_NAME;
    int ret = pam_start(serviceName, username.toLocal8Bit().data(), &conv, &m_pamHandle);

    if (ret != PAM_SUCCESS) {
        qDebug() << "pam_start() failed: " << pam_strerror(m_pamHandle, ret);
    }

    int rc = pam_authenticate(m_pamHandle, 0);
    if (rc != PAM_SUCCESS) {
        qDebug() << "pam_authenticate() failed: " << pam_strerror(m_pamHandle, rc);
    }

    int re = pam_end(m_pamHandle, rc);
    if (re != PAM_SUCCESS) {
        qDebug() << "pam_end() failed: " << pam_strerror(m_pamHandle, re);
    }

    bool is_success = (rc == PAM_SUCCESS) && (re == PAM_SUCCESS);

    // 认证成功与否，均点亮屏幕
    system("xset dpms force on");

    m_isCondition = true;
    emit respondResult(is_success ? "success" : QString());
}

int AuthAgent::GetAuthType()
{
    return m_authType;
}

int AuthAgent::pamConversation(int num_msg, const struct pam_message **msg,
                               struct pam_response **resp, void *app_data)
{
    AuthAgent *app_ptr = static_cast<AuthAgent *>(app_data);
    struct pam_response *aresp = nullptr;
    int idx = 0;
    AuthFlag auth_type = AuthFlag::Password;

    QPointer<AuthAgent> isThreadAlive(app_ptr);
    if (!isThreadAlive) {
        qDebug() << "pam: application is null";
        return PAM_CONV_ERR;
    }

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
        return PAM_CONV_ERR;

    if ((aresp = static_cast<struct pam_response*>(calloc(num_msg, sizeof(*aresp)))) == nullptr)
        return PAM_BUF_ERR;

    for (idx = 0; idx < num_msg; ++idx) {
        switch(PAM_MSG_MEMBER(msg, idx, msg_style)) {

        case PAM_PROMPT_ECHO_OFF: {
            while(app_ptr->m_isCondition) sleep(1);
            app_ptr->m_isCondition = true;

            if (!QPointer<DeepinAuthFramework>(app_ptr->deepinAuth())) {
                qDebug() << "pam: deepin auth framework is null";
                return PAM_CONV_ERR;
            }

            QString password = app_ptr->deepinAuth()->RequestEchoOff(PAM_MSG_MEMBER(msg, idx, msg));
            aresp[idx].resp = strdup(password.toLocal8Bit().data());

            if (aresp[idx].resp == nullptr)
              goto fail;

            auth_type = AuthFlag::Password;
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }

        case PAM_PROMPT_ECHO_ON:
        case PAM_ERROR_MSG:{
            qDebug() << "pam authagent error: " << PAM_MSG_MEMBER(msg, idx, msg);
            app_ptr->displayErrorMsg(QString::fromLocal8Bit(PAM_MSG_MEMBER(msg, idx, msg)));
            auth_type = AuthFlag::Fingerprint;
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }

        case PAM_TEXT_INFO: {
            qDebug() << "pam authagent info: " << PAM_MSG_MEMBER(msg, idx, msg);
            app_ptr->displayTextInfo(QString::fromLocal8Bit(PAM_MSG_MEMBER(msg, idx, msg)));
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
         }

        default:
            goto fail;
        }
    }

    *resp = aresp;
    if (auth_type == AuthFlag::Password) {
        app_ptr->m_authType = AuthFlag::Password;
    } else if (auth_type == AuthFlag::Fingerprint) {
        app_ptr->m_authType = AuthFlag::Fingerprint;
    }
    return PAM_SUCCESS;

fail:
    for (idx = 0; idx < num_msg; idx++) {
        free(aresp[idx].resp);
    }
    free(aresp);
    return PAM_CONV_ERR;
}
