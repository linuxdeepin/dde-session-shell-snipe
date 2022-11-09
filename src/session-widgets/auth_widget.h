// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHWIDGET_H
#define AUTHWIDGET_H

#include "userinfo.h"
#include "user_name_widget.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLabel>
#include <DStyleOptionButton>

#include <QWidget>

class AuthSingle;
class AuthIris;
class AuthFace;
class AuthUKey;
class AuthFingerprint;
class AuthPassword;
class AuthCustom;
class DLineEditEx;
class FrameDataBind;
class KbLayoutWidget;
class KeyboardMonitor;
class SessionBaseModel;
class UserAvatar;

DWIDGET_USE_NAMESPACE

class TransparentButton : public DFloatingButton
{
    Q_OBJECT

public:
    explicit TransparentButton(QWidget *parent = nullptr)
        : DFloatingButton (parent) {

    };

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event)

        // 按钮不可用时颜色还是使用活动色，然后需要40%透明
        DStylePainter p(this);
        DStyleOptionButton opt;
        initStyleOption(&opt);
        if (isEnabled()) {
            p.setOpacity(1.0);
        } else {
            p.setOpacity(0.4);
        }
        p.drawControl(DStyle::CE_IconButton, opt);
    };
};

class AuthWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AuthWidget(QWidget *parent = nullptr);
    ~AuthWidget() override;

    virtual void setModel(const SessionBaseModel *model);
    virtual void setAuthType(const int type);
    virtual void setAuthState(const int type, const int state, const QString &message);
    virtual int getTopSpacing() const;

    void setAccountErrorMsg(const QString &message);
    void syncPasswordResetPasswordVisibleChanged(const QVariant &value);
    void syncResetPasswordUI();

Q_SIGNALS:
    void requestCheckAccount(const QString &account);
    void requestSetKeyboardType(const QString &key);
    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();
    void updateParentLayout();

protected:
    void showEvent(QShowEvent *event) override;

protected:
    void initUI();
    void initConnections();

    virtual void checkAuthResult(const int type, const int state);

    void setUser(std::shared_ptr<User> user);
    void setLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo);
    void setAvatar(const QString &avatar);
    void updateUserDisplayNameLabel();
    void setPasswordHint(const QString &hint);
    void setLockButtonType(const int type);

    void updatePasswordExpiredState();
    void updateExpiredState();

    void registerSyncFunctions(const QString &flag, std::function<void(QVariant)> function);
    void syncSingle(const QVariant &value);
    void syncSingleResetPasswordVisibleChanged(const QVariant &value);
    void syncAccount(const QVariant &value);
    void syncPassword(const QVariant &value);
    void syncUKey(const QVariant &value);
    int calcCurrentHeight(const int height) const;
    int getAuthWidgetHeight();

protected Q_SLOTS:
    void updateBlurEffectGeometry();

protected:
    const SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;

    DBlurEffectWidget *m_blurEffectWidget; // 模糊背景
    TransparentButton *m_lockButton;         // 解锁按钮
    UserAvatar *m_userAvatar;              // 用户头像

    DLabel *m_expiredStateLabel;           // 密码过期提示
    QSpacerItem *m_expiredSpacerItem;      // 密码过期提示与按钮的间隔
    DLineEditEx *m_accountEdit;   // 用户名输入框
    UserNameWidget *m_userNameWidget;    // 用户名

    KeyboardMonitor *m_capslockMonitor;    // 大小写
    DClipEffectWidget *m_keyboardTypeClip; // 键盘布局类型菜单边界裁剪类

    AuthSingle *m_singleAuth;           // PAM
    AuthPassword *m_passwordAuth;       // 密码
    AuthFingerprint *m_fingerprintAuth; // 指纹
    AuthUKey *m_ukeyAuth;               // UKey
    AuthFace *m_faceAuth;               // 面容
    AuthIris *m_irisAuth;               // 虹膜
    AuthCustom *m_customAuth;           // 自定义认证

    QString m_passwordHint;     // 密码提示
    QString m_keyboardType;     // 键盘布局类型
    QStringList m_keyboardList; // 键盘布局列表

    std::shared_ptr<User> m_user; // 当前用户

    QList<QMetaObject::Connection> m_connectionList;
    QMap<QString, int> m_registerFunctions;
};

#endif // AUTHWIDGET_H
