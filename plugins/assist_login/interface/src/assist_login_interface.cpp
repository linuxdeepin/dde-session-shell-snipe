// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assist_login_interface.h"
#include "QtNetwork/QLocalSocket"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <cstring>

extern "C" {

const QString SocketServiceName = "dde-assist-login";

int sendAuth(const char *user, unsigned char *pw, int len)
{

    EVP_cleanup();
    // 初始化OpenSSL库
    OpenSSL_add_all_algorithms();

    QByteArray sendData = "\ngetPrivateEncrypt: \n";
    QLocalSocket socket;
    socket.connectToServer(SocketServiceName);
    if (!socket.waitForConnected()) {
        return 0;
    }
    socket.write(sendData);
    if (!socket.waitForBytesWritten()) {
        socket.close();
        return 0;
    }
    if (!socket.waitForReadyRead()) {
        return 0;
    }
    char *data = socket.readAll().data();
    // 使用私钥字符串创建RSA结构体指针
    RSA *rsa = NULL;
    BIO *bio = NULL;
    bio = BIO_new_mem_buf((void *) data, -1);
    if (!PEM_read_bio_RSAPrivateKey(bio, &rsa, NULL, NULL)) {
        fprintf(stderr, "Error reading private key.\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }
    // 解密
    unsigned char decrypted[RSA_size(rsa)];

    int decrypted_len = RSA_private_decrypt(len, pw, decrypted, rsa, RSA_PKCS1_PADDING);
    if (decrypted_len == -1) {
        fprintf(stderr, "Decryption failed.\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }

    QByteArray users = QByteArray(user);
    QByteArray password = QByteArray((char *) decrypted, len);
    QByteArray sendData1 = "\nsendAuth:" + users + "," + password + "\n";

    int account = socket.write(sendData1);
    std::cout << account << std::endl;
    if (!socket.waitForBytesWritten()) {
        socket.close();
        return 0;
    }
    socket.close();
    return 1;
}

int authServiceStarted()
{
    QLocalSocket socket;
    socket.connectToServer(SocketServiceName);
    if (socket.waitForConnected(500)) {
        // Local server is running
        socket.disconnectFromServer();
        return true;
    }
    // Local server is not running
    return false;
}

const char *getPublicEncrypt()
{
    QByteArray sendData = "\ngetPublicEncrypt: \n";
    QLocalSocket socket;
    socket.connectToServer(SocketServiceName);
    if (!socket.waitForConnected()) {
        return "";
    }
    socket.write(sendData);
    if (!socket.waitForBytesWritten()) {
        socket.close();
        return "";
    }
    if (!socket.waitForReadyRead()) {
        return "";
    }
    QByteArray byteArray = socket.readAll().data();
    const char * data = new char[byteArray.count()];
    std::strcpy(const_cast<char*>(data), byteArray.data());
    socket.close();
    return data;
}

}

