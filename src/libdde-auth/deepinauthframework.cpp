// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "deepinauthframework.h"
#include "encrypt_helper.h"

#include "authcommon.h"
#include "public_func.h"

#include <dlfcn.h>

#include <QThread>
#include <QTimer>

#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <unistd.h>

#ifdef PAM_SUN_CODEBASE
#define PAM_MSG_MEMBER(msg, n, member) ((*(msg))[(n)].member)
#else
#define PAM_MSG_MEMBER(msg, n, member) ((msg)[(n)]->member)
#endif

#define PAM_SERVICE_SYSTEM_NAME "password-auth"
#define PAM_SERVICE_DEEPIN_NAME "common-auth"

using namespace AuthCommon;

DeepinAuthFramework::DeepinAuthFramework(QObject *parent)
    : QObject(parent)
    , m_authenticateInter(new AuthInter(AUTHENTICATE_SERVICE, "/com/deepin/daemon/Authenticate", QDBusConnection::systemBus(), this))
    , m_PAMAuthThread(0)
    , m_authenticateControllers(new QMap<QString, AuthControllerInter *>())
    , m_cancelAuth(false)
    , m_waitToken(true)
{
    connect(m_authenticateInter, &AuthInter::FrameworkStateChanged, this, &DeepinAuthFramework::FramworkStateChanged);
    connect(m_authenticateInter, &AuthInter::LimitUpdated, this, &DeepinAuthFramework::LimitsInfoChanged);
    connect(m_authenticateInter, &AuthInter::SupportedFlagsChanged, this, &DeepinAuthFramework::SupportedMixAuthFlagsChanged);
    connect(m_authenticateInter, &AuthInter::SupportEncryptsChanged, this, &DeepinAuthFramework::SupportedEncryptsChanged);
}

DeepinAuthFramework::~DeepinAuthFramework()
{
    for (const QString &key : m_authenticateControllers->keys()) {
        m_authenticateControllers->remove(key);
    }
    delete m_authenticateControllers;
    DestroyAuthenticate();
}

/**
 * @brief 创建一个 PAM 认证服务的线程，在线程中等待用户输入密码
 *
 * @param account
 */
void DeepinAuthFramework::CreateAuthenticate(const QString &account)
{
    if (account == m_account && m_PAMAuthThread != 0) {
        return;
    }
    qInfo() << "Create PAM authenticate thread:" << account << m_PAMAuthThread;
    m_account = account;
    DestroyAuthenticate();
    m_cancelAuth = false;
    m_waitToken = true;
    int rc = pthread_create(&m_PAMAuthThread, nullptr, &PAMAuthWorker, this);

    if (rc != 0) {
        qCritical() << "failed to create the authentication thread: %s" << strerror(errno);
    }
}

/**
 * @brief 传入用户名
 *
 * @param arg   当前对象的指针
 * @return void*
 */
void *DeepinAuthFramework::PAMAuthWorker(void *arg)
{
    DeepinAuthFramework *authFramework = static_cast<DeepinAuthFramework *>(arg);
    if (authFramework != nullptr) {
        authFramework->PAMAuthentication(authFramework->m_account);
    } else {
        qCritical() << "pam auth worker deepin framework is nullptr";
    }
    return nullptr;
}

/**
 * @brief 执行 PAM 认证
 *
 * @param account
 */
void DeepinAuthFramework::PAMAuthentication(const QString &account)
{
    pam_handle_t *m_pamHandle = nullptr;
    pam_conv conv = {PAMConversation, static_cast<void *>(this)};
    const char *serviceName = isDeepinAuth() ? PAM_SERVICE_DEEPIN_NAME : PAM_SERVICE_SYSTEM_NAME;

    int ret = pam_start(serviceName, account.toLocal8Bit().data(), &conv, &m_pamHandle);
    if (ret != PAM_SUCCESS) {
        qCritical() << "PAM start failed:" << pam_strerror(m_pamHandle, ret) << ret;
    } else {
        qDebug() << "PAM start...";
    }

    int rc = pam_authenticate(m_pamHandle, 0);
    if (rc != PAM_SUCCESS) {
        qWarning() << "PAM authenticate failed:" << pam_strerror(m_pamHandle, rc) << rc;
        if (m_message.isEmpty()) m_message = pam_strerror(m_pamHandle, rc);
    } else {
        qDebug() << "PAM authenticate finished.";
    }

    int re = pam_end(m_pamHandle, rc);
    if (re != PAM_SUCCESS) {
        qCritical() << "PAM end failed:" << pam_strerror(m_pamHandle, re) << re;
    } else {
        qDebug() << "PAM end...";
    }

    if (rc == 0) {
        UpdateAuthState(AS_Success, m_message);
    } else {
        UpdateAuthState(AS_Failure, m_message);
    }

    m_PAMAuthThread = 0;
    system("xset dpms force on");
}

/**
 * @brief PAM 的回调函数，传入密码与各种异常处理
 *
 * @param num_msg
 * @param msg
 * @param resp
 * @param app_data  当前对象指针
 * @return int
 */
int DeepinAuthFramework::PAMConversation(int num_msg, const pam_message **msg, pam_response **resp, void *app_data)
{
    DeepinAuthFramework *app_ptr = static_cast<DeepinAuthFramework *>(app_data);
    struct pam_response *aresp = nullptr;
    int idx = 0;

    QPointer<DeepinAuthFramework> isThreadAlive(app_ptr);
    if (!isThreadAlive) {
        qWarning() << "pam: application is null";
        return PAM_CONV_ERR;
    }

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG) {
        qWarning() << "PAM_CONV_ERR :" << num_msg;
        return PAM_CONV_ERR;
    }

    if ((aresp = static_cast<struct pam_response *>(calloc(static_cast<size_t>(num_msg), sizeof(*aresp)))) == nullptr) {
        qWarning() << "PAM_BUF_ERR";
        return PAM_BUF_ERR;
    }
    const QString message = QString::fromLocal8Bit(PAM_MSG_MEMBER(msg, idx, msg));
    for (idx = 0; idx < num_msg; ++idx) {
        switch (PAM_MSG_MEMBER(msg, idx, msg_style)) {
        case PAM_PROMPT_ECHO_ON:
        case PAM_PROMPT_ECHO_OFF: {
            qDebug() << "pam auth echo:" << message;
            app_ptr->UpdateAuthState(AS_Prompt, message);
            /* 等待用户输入密码 */
            while (app_ptr->m_waitToken) {
                qDebug() << "Waiting for the password...";
                if (app_ptr->m_cancelAuth) {
                    app_ptr->m_cancelAuth = false;
                    return PAM_ABORT;
                }
                sleep(1);
            }
            app_ptr->m_waitToken = true;

            if (!QPointer<DeepinAuthFramework>(app_ptr)) {
                qCritical() << "pam: deepin auth framework is null";
                return PAM_CONV_ERR;
            }

            aresp[idx].resp = strdup(app_ptr->m_token.toLocal8Bit().data());

            if (aresp[idx].resp == nullptr) {
                goto fail;
            }

            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }
        case PAM_ERROR_MSG: {
            qDebug() << "pam auth error: " << message;
            app_ptr->m_message = message;
            app_ptr->UpdateAuthState(AS_Failure, message);
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }
        case PAM_TEXT_INFO: {
            qDebug() << "pam auth info: " << message;
            app_ptr->m_message = message;
            app_ptr->UpdateAuthState(AS_Prompt, message);
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }
        default:
            goto fail;
        }
    }

    *resp = aresp;

    return PAM_SUCCESS;

fail:
    for (idx = 0; idx < num_msg; idx++) {
        free(aresp[idx].resp);
    }
    free(aresp);
    return PAM_CONV_ERR;
}

/**
 * @brief 传入用户输入的密码（密码、PIN 等）
 *
 * @param token
 */
void DeepinAuthFramework::SendToken(const QString &token)
{
    if (!m_waitToken) {
        return;
    }
    qInfo() << "Send token to PAM";
    m_token = token;
    m_waitToken = false;
}

/**
 * @brief 更新 PAM 认证状态
 *
 * @param state
 * @param message
 */
void DeepinAuthFramework::UpdateAuthState(const int state, const QString &message)
{
    emit AuthStateChanged(AT_PAM, state, message);
}

/**
 * @brief 结束 PAM 认证服务
 */
void DeepinAuthFramework::DestroyAuthenticate()
{
    if (m_PAMAuthThread == 0) {
        return;
    }
    qInfo() << "Destroy PAM authenticate thread";
    m_cancelAuth = true;
    pthread_cancel(m_PAMAuthThread);
    pthread_join(m_PAMAuthThread, nullptr);
    m_PAMAuthThread = 0;
}

/**
 * @brief 创建认证服务
 *
 * @param account     用户名
 * @param authType    认证方式（多因、单因，一种或多种）
 * @param encryptType 加密方式
 */
void DeepinAuthFramework::CreateAuthController(const QString &account, const int authType, const int appType)
{
    if (m_authenticateControllers->contains(account) && m_authenticateControllers->value(account)->isValid()) {
        return;
    }
    const QString authControllerInterPath = m_authenticateInter->Authenticate(account, authType, appType);
    qInfo() << "Create Authenticate Session:" << account << authType << appType << authControllerInterPath;
    AuthControllerInter *authControllerInter = new AuthControllerInter("com.deepin.daemon.Authenticate", authControllerInterPath, QDBusConnection::systemBus(), this);
    m_authenticateControllers->insert(account, authControllerInter);

    connect(authControllerInter, &AuthControllerInter::FactorsInfoChanged, this, &DeepinAuthFramework::FactorsInfoChanged);
    connect(authControllerInter, &AuthControllerInter::IsFuzzyMFAChanged, this, &DeepinAuthFramework::FuzzyMFAChanged);
    connect(authControllerInter, &AuthControllerInter::IsMFAChanged, this, &DeepinAuthFramework::MFAFlagChanged);
    connect(authControllerInter, &AuthControllerInter::PINLenChanged, this, &DeepinAuthFramework::PINLenChanged);
    connect(authControllerInter, &AuthControllerInter::PromptChanged, this, &DeepinAuthFramework::PromptChanged);
    connect(authControllerInter, &AuthControllerInter::Status, this, [this](int flag, int state, const QString &msg) {
        emit AuthStateChanged(flag, state, msg);

        // 当人脸或者虹膜认证成功 或者 指纹识别失败/成功 时唤醒屏幕
        if (((AT_Face == flag || AT_Iris == flag) && AS_Success == state)
            || (AT_Fingerprint == flag && (AS_Failure == state || AS_Success == state))) {
            system("xset dpms force on");
        }
    });

    emit SessionCreated();
    emit MFAFlagChanged(authControllerInter->isMFA());
    emit FactorsInfoChanged(authControllerInter->factorsInfo());
    emit FuzzyMFAChanged(authControllerInter->isFuzzyMFA());
    emit PINLenChanged(authControllerInter->pINLen());
    emit PromptChanged(authControllerInter->prompt());


    int DAEncryptType;
    ArrayInt DAEncryptMethod;
    QString publicKey;
    // 获取非对称加密公钥
    QDBusReply<int> reply = authControllerInter->EncryptKey(
        EncryptHelper::ref().encryptType(),
        EncryptHelper::ref().encryptMethod(),
        DAEncryptMethod,
        publicKey);
    DAEncryptType = reply.value();

    // 使用DA返回的加密算法； 例如，如果是没有适配SM2算法的DA，那么就使用RSA
    EncryptHelper::ref().setEncryption(DAEncryptType, DAEncryptMethod);
    if (publicKey.isEmpty()) {
        qCritical() << "Failed to get the public key!";
        return;
    }
    EncryptHelper::ref().setPublicKey(publicKey);
    EncryptHelper::ref().initEncryptionService();
    m_authenticateControllers->value(account)->SetSymmetricKey(EncryptHelper::ref().encryptSymmetricalKey());
}

/**
 * @brief 销毁认证服务，下次使用认证服务前需要先创建
 *
 * @param account 用户名
 */
void DeepinAuthFramework::DestroyAuthController(const QString &account)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    AuthControllerInter *authControllerInter = m_authenticateControllers->value(account);
    qInfo() << "Destroy Authenticate Session:" << account << authControllerInter->path();
    authControllerInter->End(AT_All);
    authControllerInter->Quit();
    m_authenticateControllers->remove(account);
    delete authControllerInter;
    EncryptHelper::ref().releaseResources();
}

/**
 * @brief 开启认证服务。成功开启返回0,否则返回失败个数。
 *
 * @param account   帐户
 * @param authType  认证类型（可传入一种或多种）
 * @param timeout   设定超时时间（默认 -1）
 */
void DeepinAuthFramework::StartAuthentication(const QString &account, const int authType, const int timeout)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    int ret = m_authenticateControllers->value(account)->Start(authType, timeout);
    qInfo() << "Start Authenticate Session:" << account << authType << ret;
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务（认证成功或认证失败达到一定次数均会调用此方法）
 *
 * @param account   账户
 * @param authType  认证类型
 */
void DeepinAuthFramework::EndAuthentication(const QString &account, const int authType)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    qInfo() << "End Authentication:" << account << authType;
    m_authenticateControllers->value(account)->End(authType).waitForFinished();
}

/**
 * @brief 将密文发送给认证服务 -- PAM
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void DeepinAuthFramework::SendTokenToAuth(const QString &account, const int authType, const QString &token)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    qInfo() << "Send token to authentication:" << account << ", authType" << authType;

    QByteArray ba = EncryptHelper::ref().getEncryptedToken(token);
    m_authenticateControllers->value(account)->SetToken(authType, ba);
}

/**
 * @brief 设置认证服务退出的方式。
 * AutoQuit: 调用 End 并传入 -1,即可自动退出认证服务；
 * ManualQuit: 调用 End 结束本次认证后，手动调用 Quit 才能退出认证服务。
 * @param flag
 */
void DeepinAuthFramework::SetAuthQuitFlag(const QString &account, const int flag)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    m_authenticateControllers->value(account)->SetQuitFlag(flag);
}

/**
 * @brief 支持的多因子类型
 *
 * @return int
 */
int DeepinAuthFramework::GetSupportedMixAuthFlags() const
{
    return m_authenticateInter->supportedFlags();
}

/**
 * @brief 一键登录相关
 *
 * @param flag
 * @return QString
 */
QString DeepinAuthFramework::GetPreOneKeyLogin(const int flag) const
{
    return m_authenticateInter->PreOneKeyLogin(flag);
}

/**
 * @brief 获取认证框架类型
 *
 * @return int
 */
int DeepinAuthFramework::GetFrameworkState() const
{
    return m_authenticateInter->frameworkState();
}

/**
 * @brief 支持的加密类型
 *
 * @return QString 加密类型
 */
QString DeepinAuthFramework::GetSupportedEncrypts() const
{
    return m_authenticateInter->supportEncrypts();
}

/**
 * @brief 获取账户被限制时间
 *
 * @param account   账户
 * @return QString  时间
 */
QString DeepinAuthFramework::GetLimitedInfo(const QString &account) const
{
    return m_authenticateInter->GetLimits(account);
}

/**
 * @brief 获取多因子标志位，用于判断是多因子还是单因子认证。
 *
 * @param account   账户
 * @return int
 */
int DeepinAuthFramework::GetMFAFlag(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return 0;
    }
    return m_authenticateControllers->value(account)->isMFA();
}

/**
 * @brief 获取 PIN 码的最大长度，用于限制输入的 PIN 码长度。
 *
 * @param account
 * @return int
 */
int DeepinAuthFramework::GetPINLen(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return 0;
    }
    return m_authenticateControllers->value(account)->pINLen();
}

/**
 * @brief 模糊多因子信息，供 PAM 使用，这里暂时用不上
 *
 * @param account   账户
 * @return int
 */
int DeepinAuthFramework::GetFuzzyMFA(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return 0;
    }
    return m_authenticateControllers->value(account)->isFuzzyMFA();
}

/**
 * @brief 获取总的提示信息，后续可能会融合进 status。
 *
 * @param account
 * @return QString
 */
QString DeepinAuthFramework::GetPrompt(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return QString();
    }
    return m_authenticateControllers->value(account)->prompt();
}

/**
 * @brief 设置进程 path 仅开启密码认证 --- lightdm
 *
 * @param account
 * @param path
 * @return
 */
bool DeepinAuthFramework::SetPrivilegesEnable(const QString &account, const QString &path)
{
    if (!m_authenticateControllers->contains(account)) {
        return false;
    }

    QDBusPendingReply<bool> reply = m_authenticateControllers->value(account)->PrivilegesEnable(path);
    reply.waitForFinished();
    if(reply.isError()) {
        return false;
    }
    return reply.value();
}

/**
 * @brief 取消 lightdm 仅开启密码认证的限制
 *
 * @param account
 */
void DeepinAuthFramework::SetPrivilegesDisable(const QString &account)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    m_authenticateControllers->value(account)->PrivilegesDisable();
}

/**
 * @brief 获取多因子信息，返回结构体数组，包含多因子认证所有信息。
 *
 * @param account
 * @return MFAInfoList
 */
MFAInfoList DeepinAuthFramework::GetFactorsInfo(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return MFAInfoList();
    }
    return m_authenticateControllers->value(account)->factorsInfo();
}

/**
 * @brief 获取认证服务的路径
 *
 * @param account  账户
 * @return QString 认证服务路径
 */
QString DeepinAuthFramework::AuthSessionPath(const QString &account) const
{
    if (!m_authenticateControllers->contains(account)) {
        return QString();
    }
    return m_authenticateControllers->value(account)->path();
}

bool DeepinAuthFramework::authSessionExist(const QString &account) const
{
    return m_authenticateControllers->contains(account) && m_authenticateControllers->value(account)->isValid();
}

/**
 * @brief DeepinAuthFramework::isDeepinAuthValid
 * 判断DA服务是否存在以及DA服务是否可用
 * @return DA认证是否可用
 */
bool DeepinAuthFramework::isDeepinAuthValid() const
{
    qDebug() << Q_FUNC_INFO
             << ", frameworkState" << m_authenticateInter->frameworkState()
             << ", isServiceRegistered: " << QDBusConnection::systemBus().interface()->isServiceRegistered(AUTHENTICATE_SERVICE);
    return QDBusConnection::systemBus().interface()->isServiceRegistered(AUTHENTICATE_SERVICE)
            && Available == GetFrameworkState();
}
