/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lixin <lixin_cm@deepin.com>
 *
 * Maintainer: lixin <lixin_cm@deepin.com>
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

#ifndef USERLOGINWIDGET_H
#define USERLOGINWIDGET_H

#include "sessionbasemodel.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLineEdit>

using DClipEffectWidget = Dtk::Widget::DClipEffectWidget;

DWIDGET_USE_NAMESPACE

class QLabel;
class QPushButton;
class User;
class UserAvatar;
class LoginButton;
class LockPasswordWidget;
class FrameDataBind;
class OtherUserInput;
class QVBoxLayout;
class KbLayoutWidget;
class KeyboardMonitor;
class DPasswordEditEx;
class DLineEditEx;
class AuthenticationModule;
class AuthSingle;

//按UI设置提供的大小设置用户界面尺寸
const int UserFrameHeight = 167;
const int UserFrameWidth = 226;
const int UserFrameSpaceing = 40;
const int UserNameHeight = 42;

class UserLoginWidget : public QWidget
{
    Q_OBJECT
public:
    enum WidgetType {
        LoginType,   // 登录
        UserListType // 多用户
    };
    Q_ENUM(WidgetType)

    explicit UserLoginWidget(const SessionBaseModel *model, const WidgetType widgetType = LoginType, QWidget *parent = nullptr);
    ~UserLoginWidget() override;

    void setWidgetType(const int type) { Q_UNUSED(type) }             // TODO
    void setUser(const std::shared_ptr<User> user) { Q_UNUSED(user) } // TODO

    void setFaildTipMessage(const QString &message);

    inline bool isSelected() const { return m_isSelected; }
    void setSelected(bool isSelected);
    void setFastSelected(bool isSelected);

    void setUid(const uint uid);
    inline uint uid() const { return m_uid; }
    void ShutdownPrompt(SessionBaseModel::PowerAction action);

signals:
    void requestStartAuthentication(const QString &account, const int authType);            // 开启某一种认证
    void sendTokenToAuth(const QString &account, const int authType, const QString &token); // 将密文发送给认证服务
    void requestAuthUser(const QString &account, const QString &password);
    void requestCheckAccount(const QString &account);
    void clicked();
    void authFininshed(const int status);
    /////////////////////////////////////////////////////////////////////////
    void requestUserKBLayoutChanged(const QString &layout);
    void unlockActionFinish();

public slots:
    void updateExpiredStatus();
    void updateWidgetShowType(const int type);
    void updateAuthResult(const int type, const int status, const QString &message);
    void updateBlurEffectGeometry();
    void updateAvatar(const QString &path);
    void updateName(const QString &name);
    void updateLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo);
    void updateAuthStatus();
    void clearAuthStatus();
    void updateLoginState(const bool loginState);
    void updateKeyboardInfo(const QString &text);
    void updateKeyboardList(const QStringList &list);
    void updateNextFocusPosition();
    /////////////////////////////////////////////////////
    void updateAuthType(SessionBaseModel::AuthType type);
    void unlockSuccessAni(); // obsolete
    void unlockFailedAni();  // obsolete
    void updateAccoutLocale();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void initUI();
    void initConnections();

    void initSingleAuth(const int index);
    void initPasswdAuth(const int index);
    void initFingerprintAuth(const int index);
    void initFaceAuth(const int index);
    void initActiveDirectoryAuth(const int index);
    void initUkeyAuth(const int index);
    void initFingerVeinAuth(const int index);
    void initIrisAuth(const int index);
    void initPINAuth(const int index);

    void onOtherPageFocusChanged(const QVariant &value);
    void onOtherPageSingleChanged(const QVariant &value);
    void onOtherPageAccountChanged(const QVariant &value);
    void onOtherPagePINChanged(const QVariant &value);
    void onOtherPageUKeyChanged(const QVariant &value);
    void onOtherPagePasswordChanged(const QVariant &value);
    void onOtherPageKBLayoutChanged(const QVariant &value);

    void showKeyboardList();
    void updateKeyboardListPosition();

    void updateNameLabel(const QFont &font);
    void updateClipPath();

    void checkAuthResult(const int type, const int status);

private:
    const SessionBaseModel *const m_model; // 数据
    WidgetType m_widgetType;               // 窗体显示模式，正常登录样式和多用户选择列表样式

    DBlurEffectWidget *m_blurEffectWidget; // 模糊背景
    QVBoxLayout *m_userLoginLayout;        // 登录界面布局
    UserAvatar *m_userAvatar;              // 用户头像
    QWidget *m_nameWidget;                 // 用户名控件
    QLabel *m_nameLabel;                   // 用户名
    QLabel *m_loginStateLabel;             // 用户登录状态
    DLineEditEx *m_accountEdit;            // 用户名输入框
    QLabel *m_expiredStatusLabel;          // 密码过期提示
    DFloatingButton *m_lockButton;         // 解锁按钮

    AuthSingle *m_singleAuth;
    AuthenticationModule *m_passwordAuth;        // 密码
    AuthenticationModule *m_fingerprintAuth;     // 指纹
    AuthenticationModule *m_faceAuth;            // 面容
    AuthenticationModule *m_ukeyAuth;            // UKey
    AuthenticationModule *m_activeDirectoryAuth; // AD域
    AuthenticationModule *m_fingerVeinAuth;      // 指静脉
    AuthenticationModule *m_irisAuth;            // 虹膜
    AuthenticationModule *m_PINAuth;             // PIN

    ///////////////////////////////////////

    DPasswordEditEx *m_passwordEdit;          // 密码输入框
    LockPasswordWidget *m_lockPasswordWidget; // 密码锁定后，错误信息提示

    DArrowRectangle *m_kbLayoutBorder; // 键盘布局异性框类
    KbLayoutWidget *m_kbLayoutWidget;  // 键盘布局窗体
    DClipEffectWidget *m_kbLayoutClip; // 键盘布局裁减类

    QMap<QString, int> m_registerFunctionIndexs; // 数据同步

    SessionBaseModel::AuthType m_authType; // 认证类型

    bool m_isLock;     // 解锁功能是否被锁定(连续5次密码输入错误锁定)
    bool m_loginState; // 是否登录（UserFrame中使用）
    bool m_isSelected;
    bool m_isLockNoPassword;
    KeyboardMonitor *m_capslockMonitor;
    uint m_uid;
    bool m_isAlertMessageShow; //判断密码错误提示是否显示
    QString m_name;
    QTimer *m_aniTimer;                   //切换图标计时器
    QMetaObject::Connection m_connection; //定時器connection
    int m_timerIndex = 0;
    int m_action; //重启或关机行为记录
    QString m_local;
    QStringList m_keyboardList;
    QString m_keyboardInfo;

    QList<QWidget *> m_widgetsList;
};

#endif // USERLOGINWIDGET_H
