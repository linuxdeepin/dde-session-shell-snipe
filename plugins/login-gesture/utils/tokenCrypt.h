// SPDX-FileCopyrightText: 2011 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef TOKENCRYPT_H
#define TOKENCRYPT_H

#include <QString>
#include <QObject>

#include <unistd.h>
#include <random>

namespace gestureEncrypt {
/**
 * @brief crypt函数是否支持SM3算法
 * crypt使用固定的key和salt进行加密，把加密结果与已知的正确结果进行比对，相等则支持，反之不支持
 * @return true 支持
 * @return false 不支持
 */
inline bool supportSM3()
{
    char password[] = "Hello world!";
    char salt[] = "$8$saltstring";
    const QString cryptResult = "$8$saltstring$6RCuSuWbADZmLALkvsvtcYYzhrw3xxpuDcqwdPIWxTD";
    return crypt(password, salt) == cryptResult;
}

inline const QByteArray getCryptSalt(const QByteArray &key)
{
    QByteArray retsult = "";
    int count = 0;
    for (const auto value : key) {
        if (value == '$') {
            retsult += value;
            count++;
            if (count == 3) {
                return retsult;
            }
        } else {
            if (count > 0) {
                retsult += value;
            }
        }
    }

    return retsult;
}

inline QString cryptUserPassword(const QString &password, const QByteArray &salt)
{
    return crypt(password.toUtf8().data(), salt);
}

inline QString getSalt()
{
    /*
        NOTE(kirigaya): Password is a combination of salt and crypt function.
        slat is begin with $6$, 16 byte of random values, at the end of $.
        crypt function will return encrypted values.
     */
    const QString seedChars("./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    char salt[] = "$6$................$";
    if (supportSM3()) {
        salt[1] = '8';
    }

    std::random_device r;
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(0, seedChars.size() - 1); // seedChars.size()是64，生成随机数的范围应该写成[0, 63]。

    // Random access to a character in a restricted list
    for (int i = 0; i != 16; i++) {
        salt[3 + i] = seedChars.at(uniform_dist(e1)).toLatin1();
    }

    return salt;
}

inline QString cryptUserPassword(const QString &password)
{
    return crypt(password.toUtf8().data(), getSalt().toLatin1());
}
}; // namespace gestureEncrypt
#endif
