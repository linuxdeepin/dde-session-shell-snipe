// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <QDBusInterface>
#include <QDBusReply>

#include "assist_login_work.h"
#include "dbusconstant.h"

const QString SocketServiceName = "dde-assist-login";

dss::module::AssistLoginWork::AssistLoginWork(QObject *parent)
    : QObject(parent)
    , m_socketHelper(nullptr)
    , m_keyPair(nullptr)
{
    init();
}

dss::module::AssistLoginWork::~AssistLoginWork()
{
    if (m_socketHelper) {
        delete m_socketHelper;
        m_socketHelper = nullptr;
    }
}

void dss::module::AssistLoginWork::init()
{
    m_socketHelper = new SocketHelper(SocketServiceName);
    connect(m_socketHelper, &SocketHelper::createPublicEncrypt, this, &AssistLoginWork::onCreatePublicEncrypt);
    connect(m_socketHelper, &SocketHelper::createPrivateEncrypt, this, &AssistLoginWork::onCreatePrivateEncrypt);
    connect(m_socketHelper, &SocketHelper::getAuthResult, this, &AssistLoginWork::onGetAuthResult);
    connect(m_socketHelper, &SocketHelper::startAuth, this, &AssistLoginWork::onStartAuth);

    // 生成RSA的公钥和私钥
    generateRSAKeyPair();

}

void dss::module::AssistLoginWork::onCreatePublicEncrypt()
{
    if (m_socketHelper == nullptr) {
        return;
    }
    const char * key = keyToString(m_keyPair, 1);
    m_socketHelper->sendDataToClient(QByteArray(key));
    delete[] key;
}

void dss::module::AssistLoginWork::onGetAuthResult()
{
    if (m_socketHelper == nullptr) {
        return;
    }
    m_socketHelper->sendDataToClient("onGetAuthResult");
}

void dss::module::AssistLoginWork::onStartAuth(const QString &user, const QString &passwd)
{
    qDebug() << Q_FUNC_INFO << user;
    QDBusInterface dssInterface(DSS_DBUS::accountsService, DSS_DBUS::accountsPath, DSS_DBUS::accountsService,
                 QDBusConnection::systemBus());
    QDBusReply<QString> data = dssInterface.call("FindUserByName", user);
    if (!data.isValid()) {
        qWarning() << Q_FUNC_INFO << data.error().message();
        return;
    }
    QString userPath = data.value();
    if (!userPath.startsWith("/")) {
        qDebug() << Q_FUNC_INFO << " wrong account";
        return;
    }

    Q_EMIT sendAuthToken(user, passwd);
}

void dss::module::AssistLoginWork::generateRSAKeyPair()
{
    int bits = 2048;  // 设置秘钥长度

    // 创建RSA对象
    m_keyPair = RSA_new();
    if (m_keyPair == NULL) {
        return;
    }

    // 生成RSA秘钥对
    BIGNUM *e = BN_new();
    if (e == NULL) {
        RSA_free(m_keyPair);
        return;
    }

    if (BN_set_word(e, RSA_F4) != 1) {
        BN_free(e);
        RSA_free(m_keyPair);
        return;
    }

    if (RSA_generate_key_ex(m_keyPair, bits, e, NULL) != 1) {
        BN_free(e);
        RSA_free(m_keyPair);
        return;
    }

    BN_free(e);
}

std::string dss::module::AssistLoginWork::rsaEncrypt(const std::string &plaintext, RSA *publicKey)
{
    int maxEncryptSize = RSA_size(publicKey);
    std::string encryptedText;

    // 创建足够大的缓冲区来存储加密结果
    unsigned char *encryptBuffer = new unsigned char[maxEncryptSize];
    int encryptLength = RSA_public_encrypt(plaintext.length(), reinterpret_cast<const unsigned char *>(plaintext.c_str()),
                                           encryptBuffer, publicKey, RSA_PKCS1_PADDING);
    if (encryptLength != -1) {
        encryptedText.assign(reinterpret_cast<char *>(encryptBuffer), encryptLength);
    }

    delete[] encryptBuffer;
    return encryptedText;
}

std::string dss::module::AssistLoginWork::rsaDecrypt(const char *encryptedText, RSA *privateKey, int len)
{
    int maxDecryptSize = RSA_size(privateKey);
    std::string decryptedText;

    int j = 0, counter = 0;
    char c[2];
    unsigned int bytes[2];
    unsigned char UnChar[len];

    for (j = 0; j < len; j += 2) {
        if (0 == j % 2) {
            c[0] = encryptedText[j];
            c[1] = encryptedText[j + 1];
            sscanf(c, "%02x", &bytes[0]);
            UnChar[counter] = bytes[0];
            std::cout << static_cast<int>(UnChar[counter]);
            counter++;
        }
    }

    // 创建足够大的缓冲区来存储解密结果
    unsigned char *decryptBuffer = new unsigned char[maxDecryptSize];

    int decryptLength = RSA_private_decrypt(len, UnChar,
                                            decryptBuffer, privateKey, RSA_PKCS1_PADDING);
    if (decryptLength != -1) {
        decryptedText.assign(reinterpret_cast<char *>(decryptBuffer), decryptLength);
    }

    delete[] decryptBuffer;
    return decryptedText;
}

const char *dss::module::AssistLoginWork::keyToString(RSA *rsaKey, int isPublic)
{
    BIO *bio = BIO_new(BIO_s_mem());
    char *pemKey = NULL;

    if (isPublic) {
        if (PEM_write_bio_RSAPublicKey(bio, rsaKey) != 1) {
            printf("Error converting RSA public key to PEM.\n");
            BIO_free(bio);
            return NULL;
        }
    } else {
        if (PEM_write_bio_RSAPrivateKey(bio, rsaKey, NULL, NULL, 0, NULL, NULL) != 1) {
            printf("Error converting RSA private key to PEM.\n");
            BIO_free(bio);
            return NULL;
        }
    }

    int keyLength = BIO_pending(bio);
    pemKey = (char *) malloc(keyLength + 1);

    if (pemKey == NULL) {
        printf("Memory allocation error.\n");
        BIO_free(bio);
        return NULL;
    }

    if (BIO_read(bio, pemKey, keyLength) != keyLength) {
        printf("Error reading PEM key from BIO.\n");
        free(pemKey);
        pemKey = NULL;
    }

    pemKey[keyLength] = '\0';
    BIO_free(bio);
    return pemKey;
}

void dss::module::AssistLoginWork::onStopService()
{
    if (m_socketHelper) {
        delete m_socketHelper;
        m_socketHelper = nullptr;
    }
}

void dss::module::AssistLoginWork::onStartService()
{
    if (m_socketHelper == nullptr) {
        m_socketHelper = new SocketHelper(SocketServiceName);
        connect(m_socketHelper, &SocketHelper::createPublicEncrypt, this, &AssistLoginWork::onCreatePublicEncrypt);
        connect(m_socketHelper, &SocketHelper::createPrivateEncrypt, this, &AssistLoginWork::onCreatePrivateEncrypt);
        connect(m_socketHelper, &SocketHelper::getAuthResult, this, &AssistLoginWork::onGetAuthResult);
        connect(m_socketHelper, &SocketHelper::startAuth, this, &AssistLoginWork::onStartAuth);
    }
}

void dss::module::AssistLoginWork::onCreatePrivateEncrypt()
{
    if (m_socketHelper == nullptr) {
        return;
    }
    const char * key = keyToString(m_keyPair, 0);
    m_socketHelper->sendDataToClient(QByteArray(key));
    delete[] key;
}
