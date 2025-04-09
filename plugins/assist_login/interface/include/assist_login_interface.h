// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASSIST_LOGIN_INTERFACE_H
#define ASSIST_LOGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发送账号和密码进行登录认证
 * @param 本地账号账号名称，账号密码。
 * @return true表示发送认证成功，false发送认证失败，注意：该结果不代表认证结果
 */
extern int sendAuth(const char *account, unsigned char *pw, int len);

/**
 * @brief 判断认证服务是否已经开启
 * @return true表示已经开启，false表示认证服务还未开启
 */
extern int authServiceStarted();

/**
 * @brief 获取非对称加密使用的公钥
 * @return 公钥token
 */
extern const char *getPublicEncrypt();

#ifdef __cplusplus
}
#endif

#endif //ASSIST_LOGIN_INTERFACE_H
