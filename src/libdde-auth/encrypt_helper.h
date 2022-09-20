// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <openssl/aes.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef PREFER_USING_GM
#include <openssl/sm2.h>
#include <openssl/sm4.h>
#endif
#ifdef __cplusplus
}
#endif
#include <types/arrayint.h>

#include <DSingleton>

class EncryptHelper : public Dtk::Core::DSingleton<EncryptHelper>
{
    friend class Dtk::Core::DSingleton<EncryptHelper>;

public:
    enum EncryptType {
        ET_RSA = 0,
        ET_SM2
    };

    void setEncryption(const int type, ArrayInt method = {1});
    QByteArray encryptSymmetricalKey();
    QByteArray getEncryptedToken(const QString &token);
    int encryptType() const { return m_encryptType; }
    ArrayInt encryptMethod() const { return m_encryptMethod; }
    void setPublicKey(const QString &publicKey) { m_publicKey = publicKey; }
    void initEncryptionService();
    void releaseResources();

private:
    EncryptHelper();
    ~EncryptHelper();

    QByteArray RSAEncryptSymmetricalKey();
#ifdef PREFER_USING_GM
    QByteArray SM2EncryptSymmetricalKey();
#endif

private:
    BIO *m_BIO;
    RSA *m_RSA;
    EC_KEY *m_ecKey;
    int m_encryptType;
    QString m_symmetricKey;
    QString m_publicKey;
    ArrayInt m_encryptMethod;
};
