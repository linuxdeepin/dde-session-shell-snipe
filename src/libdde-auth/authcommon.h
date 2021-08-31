/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
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

namespace AuthCommon {

/**
 * @brief The AuthFrameFlag enum
 * 认证框架是否可用的标志位
 */
enum AuthFrameFlag {
    Unavailable,
    Available
};

/**
 * @brief The AppType enum
 * 发起认证的应用类型
 */
enum AppType {
    AppTypeLogin = 1, // 登录
    AppTypeLock = 2   // 锁屏
};

/**
 * @brief The InputType enum
 * 认证信息输入设备的类型
 */
enum InputType {
    InputDefault = 0, // 默认
    InputKeyboard,    // 键盘
    InputFinger,      // 指纹和指静脉
    InputCameraFace,  // 人脸摄像头
    InputCameraIris   // 虹膜摄像头
};

/**
 * @brief The AuthType enum
 * 认证类型
 */
enum AuthType {
    AuthTypeNone = 0,                 // none
    AuthTypePassword = 1 << 0,        // 密码
    AuthTypeFingerprint = 1 << 1,     // 指纹
    AuthTypeFace = 1 << 2,            // 人脸
    AuthTypeActiveDirectory = 1 << 3, // AD域
    AuthTypeUkey = 1 << 4,            // ukey
    AuthTypeFingerVein = 1 << 5,      // 指静脉
    AuthTypeIris = 1 << 6,            // 虹膜
    AuthTypePIN = 1 << 7,             // PIN
    AuthTypeSingle = 1 << 30,         // 单因/PAM
    AuthTypeAll = -1                  // all
};

/**
 * @brief The AuthStatus enum
 * 认证状态
 */
enum AuthStatus {
    StatusCodeSuccess = 0, // 成功，此次认证的最终结果
    StatusCodeFailure,     // 失败，此次认证的最终结果
    StatusCodeCancel,      // 取消，当认证没有给出最终结果时，调用 End 会出发 Cancel 信号
    StatusCodeTimeout,     // 超时
    StatusCodeError,       // 错误
    StatusCodeVerify,      // 验证中
    StatusCodeException,   // 设备异常，当前认证会被 End
    StatusCodePrompt,      // 设备提示
    StatusCodeStarted,     // 认证已启动，调用 Start 之后，每种成功开启都会发送此信号
    StatusCodeEnded,       // 认证已结束，调用 End 之后，每种成功关闭的都会发送此信号，当某种认证类型被锁定时，也会触发此信号
    StatusCodeLocked,      // 认证已锁定，当认证类型锁定时，触发此信号。该信号不会给出锁定等待时间信息
    StatusCodeRecover,     // 设备恢复，需要调用 Start 重新开启认证，对应 StatusCodeException
    StatusCodeUnlocked     // 认证解锁，对应 StatusCodeLocked
};

} // namespace AuthCommon
