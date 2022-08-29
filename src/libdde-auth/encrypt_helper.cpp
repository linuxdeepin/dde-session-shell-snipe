// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "encrypt_helper.h"
#include "public_func.h"

#include <QDebug>

#define PKCS1_HEADER "-----BEGIN RSA PUBLIC KEY-----"
#define PKCS8_HEADER "-----BEGIN PUBLIC KEY-----"

EncryptHelper::EncryptHelper()
    : m_BIO(nullptr)
    , m_RSA(nullptr)
    , m_ecKey(nullptr)
#ifndef PREFER_USING_GM
    , m_encryptType(ET_RSA)
#else
    , m_encryptType(ET_SM2)
#endif
    , m_encryptMethod({1})
{

}

EncryptHelper::~EncryptHelper()
{
    releaseResources();
}

/**
 * @brief 初始化加密服务，创建对称加密密钥
 */
void EncryptHelper::initEncryptionService()
{
    m_BIO = BIO_new(BIO_s_mem());
    BIO_puts(m_BIO, m_publicKey.toLatin1().data());

    if (ET_SM2 == m_encryptType) {
        PEM_read_bio_EC_PUBKEY(m_BIO, &m_ecKey, nullptr, nullptr);
    } else {
        if (strncmp(m_publicKey.toLatin1().data(), PKCS8_HEADER, strlen(PKCS8_HEADER)) == 0) {
            PEM_read_bio_RSA_PUBKEY(m_BIO, &m_RSA, nullptr, nullptr);
        } else if (strncmp(m_publicKey.toLatin1().data(), PKCS1_HEADER, strlen(PKCS1_HEADER)) == 0) {
            PEM_read_bio_RSAPublicKey(m_BIO, &m_RSA, nullptr, nullptr);
        }
    }

    /* 生成对称加密的密钥 */
    srand(static_cast<unsigned int>(time(nullptr)));
    int randNum = (10000000 + rand() % 10000000) % 100000000;
    m_symmetricKey = QString::number(randNum) + QString::number(randNum);
}

/**
 * @brief 设置加密类型和加密方式
 *
 * @param type 非对称加密类型
 * @param method 默认传{1}，没人知道为什么
 */
void EncryptHelper::setEncryption(const int type, ArrayInt method)
{
    qInfo() << "Set encryption type: " << type;
    m_encryptType = type;
    m_encryptMethod = method;
}

/**
 * @brief 用非对称加密公钥加密对称加密的密钥
 *
 * @return 加密后的对称加密密钥
 */
QByteArray EncryptHelper::encryptSymmetricalKey()
{
    if (ET_SM2 == m_encryptType) {
#ifdef PREFER_USING_GM
        return SM2EncryptSymmetricalKey();
#endif
    }

    return RSAEncryptSymmetricalKey();
}


QByteArray EncryptHelper::RSAEncryptSymmetricalKey()
{
    int size = RSA_size(m_RSA);
    char *ciphertext = new char[static_cast<unsigned long>(size)];
    RSA_public_encrypt(m_symmetricKey.length(), reinterpret_cast<unsigned char *>(m_symmetricKey.toLatin1().data()), reinterpret_cast<unsigned char *>(ciphertext), m_RSA, 1);
    QByteArray ba = QByteArray(ciphertext, size);
    delete[] ciphertext;
    return ba;
}


#ifdef PREFER_USING_GM
QByteArray EncryptHelper::SM2EncryptSymmetricalKey()
{
    size_t csize = 0;
    size_t psize = m_symmetricKey.length();
    if (1 != sm2_ciphertext_size(m_ecKey, EVP_sm3(), psize, &csize)) {
        qCritical() << "Can't get sm2 ciphertext size";
        return "";
    }

    uint8_t *cipherText = new uint8_t[csize];
    if (1 != sm2_encrypt(m_ecKey, EVP_sm3(), (uint8_t *)m_symmetricKey.toStdString().c_str(), psize, cipherText, &csize)) {
        qCritical() << "Can't encrypt sm2 cipher";
	    delete[] cipherText;
	    return "";
    }

    QByteArray ba(reinterpret_cast<char*>(cipherText), csize);
    delete[] cipherText;
    return ba;
}
#endif

QByteArray EncryptHelper::getEncryptedToken(const QString &token)
{
    const int tokenSize = token.size();
    const int padding = AES_BLOCK_SIZE - tokenSize % AES_BLOCK_SIZE;
    const int blockCount = token.length() / AES_BLOCK_SIZE + 1;
    const int bufferSize = blockCount * AES_BLOCK_SIZE;
    char *tokenBuffer = new char[static_cast<size_t>(bufferSize)];
    memset(tokenBuffer, padding, static_cast<size_t>(bufferSize));
    memcpy(tokenBuffer, token.toLatin1().data(), static_cast<size_t>(tokenSize));
    char *ciphertext = new char[static_cast<size_t>(bufferSize)];
    memset(ciphertext, 0, static_cast<size_t>(bufferSize));
#ifdef PREFER_USING_GM
    SM4_KEY sm4;
#endif
    AES_KEY aes;
    int ret = 0;
    if (ET_SM2 == m_encryptType) {
#ifdef PREFER_USING_GM
        SM4_set_key(reinterpret_cast<uint8_t *>(m_symmetricKey.toLatin1().data()), &sm4);
#endif
    } else {
        AES_set_encrypt_key(reinterpret_cast<unsigned char *>(m_symmetricKey.toLatin1().data()), m_symmetricKey.length() * 8, &aes);
    }
    if (ret < 0) {
        qCritical() << "Failed to set symmetric key!";
        delete[] tokenBuffer;
        delete[] ciphertext;
        return QByteArray();
    }
    unsigned char *iv = new unsigned char[AES_BLOCK_SIZE];
    memset(iv, 0, AES_BLOCK_SIZE);
    if (ET_SM2 == m_encryptType) {
#ifdef PREFER_USING_GM
        SM4_encrypt(reinterpret_cast<uint8_t *>(tokenBuffer), reinterpret_cast<uint8_t *>(ciphertext), &sm4);
#endif
    } else {
        AES_cbc_encrypt(reinterpret_cast<unsigned char *>(tokenBuffer), reinterpret_cast<unsigned char *>(ciphertext), static_cast<size_t>(bufferSize), &aes, iv, AES_ENCRYPT);
    }

    QByteArray ba = QByteArray(ciphertext, bufferSize);
    delete[] ciphertext;
    delete[] tokenBuffer;
    delete[] iv;

    return ba;
}

void EncryptHelper::releaseResources()
{
    if (m_BIO) {
        BIO_free(m_BIO);
        m_BIO = nullptr;
    }

    if (m_RSA) {
        RSA_free(m_RSA);
        m_RSA = nullptr;
    }

    if (m_ecKey) {
        EC_KEY_free(m_ecKey);
        m_ecKey = nullptr;
    }
}
