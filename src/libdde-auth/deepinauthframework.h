/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Yin Jie <yinjie@uniontech.com>
*
* Maintainer: Yin Jie <yinjie@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DEEPINAUTHFRAMEWORK_H
#define DEEPINAUTHFRAMEWORK_H

#include <QObject>
#include <QPointer>

#include <com_deepin_daemon_authenticate.h>
#include <com_deepin_daemon_authenticate_session2.h>
#include <memory>
#include <openssl/aes.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sm2.h>
#include <openssl/sm4.h>
#include <openssl/evp.h>

#define AUTHENTICATE_SERVICE "com.deepin.daemon.Authenticate"

using AuthInter = com::deepin::daemon::Authenticate;
using AuthControllerInter = com::deepin::daemon::authenticate::Session;


class DeepinAuthFramework : public QObject
{
    Q_OBJECT

public:
    enum AuthQuitFlag {
        AutoQuit,  // 自动退出（默认）
        ManualQuit // 手动退出
    };
    Q_ENUM(AuthQuitFlag)

    explicit DeepinAuthFramework(QObject *parent = nullptr);
    ~DeepinAuthFramework() override;

    /* Compatible with old authentication methods */
    void CreateAuthenticate(const QString &account);
    void SendToken(const QString &token);
    void DestroyAuthenticate();

    /* com.deepin.daemon.Authenticate */
    int GetFrameworkState() const;
    int GetSupportedMixAuthFlags() const;
    QString GetPreOneKeyLogin(const int flag) const;
    QString GetLimitedInfo(const QString &account) const;
    QString GetSupportedEncrypts() const;
    /* com.deepin.daemon.Authenticate.Session */
    int GetFuzzyMFA(const QString &account) const;
    int GetMFAFlag(const QString &account) const;
    int GetPINLen(const QString &account) const;
    MFAInfoList GetFactorsInfo(const QString &account) const;
    QString GetPrompt(const QString &account) const;
    bool SetPrivilegesEnable(const QString &account, const QString &path);
    void SetPrivilegesDisable(const QString &account);

    QString AuthSessionPath(const QString &account) const;
    void setEncryption(const int type, const ArrayInt method);
    bool authSessionExist(const QString &account) const;
    bool isDeepinAuthValid() const;

signals:
    /* com.deepin.daemon.Authenticate */
    void LimitsInfoChanged(const QString &);
    void SupportedMixAuthFlagsChanged(const int);
    void FramworkStateChanged(const int);
    void SupportedEncryptsChanged(const QString &);
    /* com.deepin.daemon.Authenticate.Session */
    void MFAFlagChanged(const bool);
    void FuzzyMFAChanged(const bool);
    void PromptChanged(const QString &);
    void FactorsInfoChanged(const MFAInfoList &);
    void PINLenChanged(const int);
    void AuthStateChanged(const int, const int, const QString &);
    void SessionCreated();

public slots:
    /* New authentication framework */
    void CreateAuthController(const QString &account, const int authType, const int appType);
    void DestroyAuthController(const QString &account);
    void StartAuthentication(const QString &account, const int authType, const int timeout);
    void EndAuthentication(const QString &account, const int authType);
    void SendTokenToAuth(const QString &account, const int authType, const QString &token);
    void SetAuthQuitFlag(const QString &account, const int flag = AutoQuit);

private:
    /* Compatible with old authentication methods */
    static void *PAMAuthWorker(void *arg);
    void PAMAuthentication(const QString &account);
    static int PAMConversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *app_data);
    void UpdateAuthState(const int state, const QString &message);

private:
    AuthInter *m_authenticateInter;
    pthread_t m_PAMAuthThread;
    QString m_account;
    QString m_message;
    QString m_token;
    int m_encryptType;
    QString m_publicKey;
    QString m_symmetricKey;
    ArrayInt m_encryptMethod;
    QMap<QString, AuthControllerInter *> *m_authenticateControllers;
    bool m_cancelAuth;
    bool m_waitToken;
};

#endif // DEEPINAUTHFRAMEWORK_H
