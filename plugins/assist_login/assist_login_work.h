// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASSIST_LOGIN_WORK_H
#define ASSIST_LOGIN_WORK_H

#include <QObject>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "socketHelper.h"

namespace dss {

namespace module {

class AssistLoginWork: public QObject
{
    Q_OBJECT
public:
    explicit AssistLoginWork(QObject *parent = nullptr);
    ~AssistLoginWork();

public Q_SLOTS:
    void onCreatePublicEncrypt();
    void onCreatePrivateEncrypt();
    void onGetAuthResult();
    void onStartAuth(const QString &user, const QString &passwd);
    void onStopService();
    void onStartService();

Q_SIGNALS:
    void sendAuthToken(const QString &user, const QString &passwd);

private:
    void init();
    // 生成RSA密钥对
    void generateRSAKeyPair();
    // RSA加密
    std::string rsaEncrypt(const std::string &plaintext, RSA *publicKey);
    // RSA解密
    std::string rsaDecrypt(const char *encryptedText, RSA *privateKey, int len);
    // 将公钥转换为字符串
    const char *keyToString(RSA *rsaKey, int isPublic);

    SocketHelper *m_socketHelper;
    RSA *m_keyPair;
};

}
}
#endif //ASSIST_LOGIN_WORK_H
