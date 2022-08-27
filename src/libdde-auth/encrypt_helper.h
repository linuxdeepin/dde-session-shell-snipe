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
