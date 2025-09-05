// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "deepinauthframework.h"
#include "encrypt_helper.h"
#include "authcommon.h"
#include "public_func.h"
#include "dbusconstant.h"

#include <dlfcn.h>

#include <security/pam_appl.h>
#include <unistd.h>

#ifdef PAM_SUN_CODEBASE
#define PAM_MSG_MEMBER(msg, n, member) ((*(msg))[(n)].member)
#else
#define PAM_MSG_MEMBER(msg, n, member) ((msg)[(n)]->member)
#endif

#define PAM_SERVICE_SYSTEM_NAME "password-auth"
#define PAM_SERVICE_DEEPIN_NAME "dde-lock"

using namespace AuthCommon;

DeepinAuthFramework::DeepinAuthFramework(QObject *parent)
    : QObject(parent)
    , m_authenticateInter(new AuthInter(DSS_DBUS::authenticateService, DSS_DBUS::authenticatePath, QDBusConnection::systemBus(), this))
    , m_PAMAuthThread(0)
    , m_authenticateControllers(new QMap<QString, AuthControllerInter *>())
    , m_cancelAuth(false)
    , m_waitToken(true)
    , m_isDAStartupCompleted(false)
{
    connect(m_authenticateInter, &AuthInter::FrameworkStateChanged, this, &DeepinAuthFramework::FramworkStateChanged);
    connect(m_authenticateInter, &AuthInter::LimitUpdated, this, &DeepinAuthFramework::LimitsInfoChanged);
    connect(m_authenticateInter, &AuthInter::SupportedFlagsChanged, this, &DeepinAuthFramework::SupportedMixAuthFlagsChanged);
    connect(m_authenticateInter, &AuthInter::SupportEncryptsChanged, this, &DeepinAuthFramework::SupportedEncryptsChanged);
    QDBusConnection::systemBus().connect(DSS_DBUS::authenticateService, DSS_DBUS::authenticatePath, DSS_DBUS::authenticateService, "DeviceChange", this, SLOT(onDeviceChanged(const int, const int)));

    if (m_authenticateInter->isValid()) {
        m_isDAStartupCompleted = true;
    } else{
        // 异步拉起DA，避免阻塞主线程
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/", "org.freedesktop.DBus", "StartServiceByName");
        message << DSS_DBUS::authenticateService << 0u;
        QDBusPendingCall async = QDBusConnection::systemBus().asyncCall(message);
        auto *watcher = new QDBusPendingCallWatcher(async);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *callWatcher) {
            QDBusPendingReply<void> reply = *callWatcher;
            if (!reply.isError()) {
                m_isDAStartupCompleted = true;
                Q_EMIT startupCompleted();
            } else {
                qCWarning(DDE_SHELL) << "DA startup has error: " << reply.error().message();
            }

            callWatcher->deleteLater();
            watcher->deleteLater();
        });
    }

    connect(&m_authReminder, &QTimer::timeout, this, []{
        qCWarning(DDE_SHELL) << "No auth result recived after token send";
    });

    m_authReminder.setSingleShot(false);
}

void DeepinAuthFramework::onDeviceChanged(const int authType, const int state)
{
    qCInfo(DDE_SHELL) << "Device changed, auth type:" << authType << ", state:" << state;
    Q_EMIT DeviceChanged(authType, state);
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
    qCInfo(DDE_SHELL) << "Create PAM authenticate thread, accout: " << account << "PAM auth thread: " << m_PAMAuthThread;
    m_account = account;
    DestroyAuthenticate();
    m_cancelAuth = false;
    m_waitToken = true;
    int rc = pthread_create(&m_PAMAuthThread, nullptr, &PAMAuthWorker, this);

    if (rc != 0) {
        qCritical() << "ERROR: failed to create the authentication thread: %s" << strerror(errno);
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
        qCritical() << "ERROR: pam auth worker deepin framework is nullptr";
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
        qCritical() << "ERROR: PAM start failed, error: " << pam_strerror(m_pamHandle, ret) << ", start infomation: " << ret;
    } else {
        qCDebug(DDE_SHELL) << "PAM start...";
    }

    int rc = pam_authenticate(m_pamHandle, 0);
    if (rc != PAM_SUCCESS) {
        qCWarning(DDE_SHELL) << "PAM authenticate failed, error: " << pam_strerror(m_pamHandle, rc) << ", PAM authenticate: " << rc;
	if (m_message.isEmpty()){
            if(rc == PAM_AUTH_ERR){
                m_message = tr("Wrong Password");
            }else{
                m_message = pam_strerror(m_pamHandle, rc);
            }
        }
    } else {
        qCDebug(DDE_SHELL) << "PAM authenticate finished.";
    }

    int re = pam_end(m_pamHandle, rc);
    if (re != PAM_SUCCESS) {
        qCritical() << "PAM end failed:" << pam_strerror(m_pamHandle, re) << "PAM end infomation: " << re;
    } else {
        qCDebug(DDE_SHELL) << "PAM end...";
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
        qCWarning(DDE_SHELL) << "PAM conversation: application is null";
        return PAM_CONV_ERR;
    }

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG) {
        qCWarning(DDE_SHELL) << "PAM conversation: message num error :" << num_msg;
        return PAM_CONV_ERR;
    }

    if ((aresp = static_cast<struct pam_response *>(calloc(static_cast<size_t>(num_msg), sizeof(*aresp)))) == nullptr) {
        qCWarning(DDE_SHELL) << "PAM conversation buffer error: " << aresp;
        return PAM_BUF_ERR;
    }
    const QString message = QString::fromLocal8Bit(PAM_MSG_MEMBER(msg, idx, msg));
    for (idx = 0; idx < num_msg; ++idx) {
        switch (PAM_MSG_MEMBER(msg, idx, msg_style)) {
        case PAM_PROMPT_ECHO_ON:
        case PAM_PROMPT_ECHO_OFF: {
            qCDebug(DDE_SHELL) << "PAM auth echo:" << message;
            app_ptr->UpdateAuthState(AS_Prompt, message);
            /* 等待用户输入密码 */
            while (app_ptr->m_waitToken) {
                qCDebug(DDE_SHELL) << "Waiting for the password...";
                if (app_ptr->m_cancelAuth) {
                    app_ptr->m_cancelAuth = false;
                    return PAM_ABORT;
                }
                sleep(1);
            }
            app_ptr->m_waitToken = true;

            if (!QPointer<DeepinAuthFramework>(app_ptr)) {
                qCritical() << "ERROR: PAM deepin auth framework is null";
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
            qCDebug(DDE_SHELL) << "PAM auth error: " << message;
            app_ptr->m_message = message;
            app_ptr->UpdateAuthState(AS_Failure, message);
            aresp[idx].resp_retcode = PAM_SUCCESS;
            break;
        }
        case PAM_TEXT_INFO: {
            qCDebug(DDE_SHELL) << "PAM auth info: " << message;
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
    qCInfo(DDE_SHELL) << "Token arrived, is it empty ? " << token.isEmpty();
    m_token = token;
    m_waitToken = false;
}

/**
 * @brief 更新 PAM 认证状态
 *
 * @param state
 * @param message
 */
void DeepinAuthFramework::UpdateAuthState(const AuthState state, const QString &message)
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
    qCInfo(DDE_SHELL) << "Destroy PAM authenticate thread";
    m_cancelAuth = true;
    pthread_cancel(m_PAMAuthThread);
    pthread_join(m_PAMAuthThread, nullptr);
    m_PAMAuthThread = 0;
}

bool DeepinAuthFramework::IsUsingPamAuth()
{
    return m_PAMAuthThread != 0;
}

/**
 * @brief 创建认证服务
 *
 * @param account     用户名
 * @param authType    认证方式（多因、单因，一种或多种）
 * @param encryptType 加密方式
 */
void DeepinAuthFramework::CreateAuthController(const QString &account, const AuthFlags authType, const int appType)
{
    if (m_authenticateControllers->contains(account) && m_authenticateControllers->value(account)->isValid()) {
        return;
    }
    const QString authControllerInterPath = m_authenticateInter->Authenticate(account, authType, appType);
    qCInfo(DDE_SHELL) << "Create auth controller, account:" << account
            << ", Auth type:"<< authType
            << ", App type:" << appType
            << ", Authentication session path: " << authControllerInterPath;
    AuthControllerInter *authControllerInter = new AuthControllerInter(DSS_DBUS::authenticateService, authControllerInterPath, QDBusConnection::systemBus(), this);
    m_authenticateControllers->insert(account, authControllerInter);

    connect(authControllerInter, &AuthControllerInter::FactorsInfoChanged, this, &DeepinAuthFramework::FactorsInfoChanged);
    connect(authControllerInter, &AuthControllerInter::IsFuzzyMFAChanged, this, &DeepinAuthFramework::FuzzyMFAChanged);
    connect(authControllerInter, &AuthControllerInter::IsMFAChanged, this, &DeepinAuthFramework::MFAFlagChanged);
    connect(authControllerInter, &AuthControllerInter::PINLenChanged, this, &DeepinAuthFramework::PINLenChanged);
    connect(authControllerInter, &AuthControllerInter::PromptChanged, this, &DeepinAuthFramework::PromptChanged);
    connect(authControllerInter, &AuthControllerInter::Status, this, [this](int flag, int state, const QString &msg) {
        if (m_authReminder.isActive()) {
            m_authReminder.stop();
            qCWarning(DDE_SHELL) << "reminder stopped for status changed";
        }

        const AuthType type = AUTH_TYPE_CAST(flag);
        const AuthState authState = AUTH_STATE_CAST(state);
        emit AuthStateChanged(type, authState, msg);

        // 当人脸或者虹膜认证成功 或者 指纹识别失败/成功 时唤醒屏幕
        if (((AT_Face == type || AT_Iris == type) && AS_Success == authState)
            || (AT_Fingerprint == type && (AS_Failure == authState || AS_Success == authState))
            || (AT_Passkey == type && (AS_Failure == authState || AS_Success == authState))) {
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
        qCritical() << "ERROR: Failed to get the public key!";
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
    qCInfo(DDE_SHELL) << "Destroy authenticate session:" << account  << ", interface path: " << authControllerInter->path();
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
void DeepinAuthFramework::StartAuthentication(const QString &account, const AuthFlags authType, const int timeout)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }

    int ret = m_authenticateControllers->value(account)->Start(authType, timeout);
    qCInfo(DDE_SHELL) << "Start authentication, account:" << account << ", auth type" << authType << ", ret:" << ret;

    m_lastAuthUser = account;
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务（认证成功或认证失败达到一定次数均会调用此方法）
 *
 * @param account   账户
 * @param authType  认证类型
 */
void DeepinAuthFramework::EndAuthentication(const QString &account, const AuthFlags authType)
{
    if (!m_authenticateControllers->contains(account)) {
        return;
    }
    qCInfo(DDE_SHELL) << "End authentication:" << ", account: " << account << "auth type: " << authType;
    m_authenticateControllers->value(account)->End(authType).waitForFinished();
}

/**
 * @brief 将密文发送给认证服务 -- PAM
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void DeepinAuthFramework::SendTokenToAuth(const QString &account, const AuthType authType, const QString &token)
{
    qCInfo(DDE_SHELL) << "Send token to auth, account: " << account << ", auth type:" << authType;
    if (!m_authenticateControllers->contains(account)) {
        qCWarning(DDE_SHELL) << "account not included";
        return;
    }

    QByteArray ba = EncryptHelper::ref().getEncryptedToken(token);
    m_authenticateControllers->value(account)->SetToken(authType, ba);
    m_authReminder.start(3000);
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
AuthFlags DeepinAuthFramework::GetSupportedMixAuthFlags() const
{
    return AUTH_FLAGS_CAST(m_authenticateInter->supportedFlags());
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
    return QDBusConnection::systemBus().interface()->isServiceRegistered(DSS_DBUS::authenticateService)
            && (Available == GetFrameworkState());
}

void DeepinAuthFramework::sendExtraInfo(const QString &account, AuthType authType, const QString &info)
{
    if (!m_authenticateControllers->contains(account)) {
        qCWarning(DDE_SHELL) << "Do not contain account";
        return;
    }
    qInfo() << "Send extra info to authentication:" << account << authType;
    AuthControllerInter *inter = m_authenticateControllers->value(account);
    QDBusMessage msg = QDBusMessage::createMethodCall(inter->service(), inter->path(), inter->interface(), "SetExtraInfo");
    msg << authType << QString("{\"type\":%1,\"data\":\"%2\"}").arg(authType).arg(info);
    QDBusPendingCall reply = inter->connection().asyncCall(msg, 2000);
    if (reply.isError()) {
        qCWarning(DDE_SHELL) << "Send extra info error:" << reply.error();
    }
}
