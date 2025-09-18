// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHPASSWORD_H
#define AUTHPASSWORD_H

#include "auth_module.h"
#include "assist_login_widget.h"
#include "passworderrortipswidget.h"

#include <DIconButton>
#include <DLabel>
#include <DPushButton>
#include <DFloatingMessage>
#include <DSuggestButton>
#include <DMessageManager>
#include <DAlertControl>

#define Password_Auth QStringLiteral(":/misc/images/auth/password.svg")

class DLineEditEx;
DWIDGET_USE_NAMESPACE

class AuthPassword : public AuthModule
{
    Q_OBJECT
public:
    explicit AuthPassword(QWidget *parent = nullptr);
    ~AuthPassword() override;

    void reset();
    QString lineEditText() const;

    inline bool passwordLineEditEnabled() const { return m_passwordLineEditEnabled; }
    void setPasswordLineEditEnabled(const bool enable);

    void setAnimationState(const bool start) override;
    void setAuthState(const AuthCommon::AuthState state, const QString &result) override;
    void setCapsLockVisible(const bool on);
    void setLimitsInfo(const LimitsInfo &info) override;
    void setLineEditEnabled(const bool enable);
    void setLineEditInfo(const QString &text, const TextType type);
    void setPasswordHint(const QString &hint);
    void setCurrentUid(uid_t uid);
    void showResetPasswordMessage();
    void closeResetPasswordMessage();
    void setAuthStatueVisible(bool visible);

    bool isShowResetPasswordMessage();
    void updatePluginConfig();
    void startPluginAuth();
    void hidePlugin();

    bool isPasswdAuthWidgetReplaced() const
    {
        return m_isPasswdAuthWidgetReplaced;
    }

    void setPasswdAuthWidgetReplaced(bool isPasswdAuthWidgetReplaced)
    {
        m_isPasswdAuthWidgetReplaced = isPasswdAuthWidgetReplaced;
    }
    bool isShowPasswrodErrorTip();
    inline bool canShowPasswrodErrorTip() { return m_canShowPasswordErrorTips; }
    void showErrorTip(const QString &text);
    void clearPasswrodErrorTip(bool isClear);
    void updatePasswrodErrorTipUi();

    AssistLoginWidget * assistLoginWidget() const { return m_assistLoginWidget; }

signals:
    void focusChanged(const bool);
    void lineEditTextChanged(const QString &); // 数据同步
    void requestChangeFocus();                 // 切换焦点
    void requestShowKeyboardList();            // 显示键盘布局列表
    void resetPasswordMessageVisibleChanged(const bool isVisible);
    void notifyLockedStateChanged(bool isLocked);
    void requestPluginConfigChanged(const LoginPlugin::PluginConfig &PluginConfig);
    void requestHidePlugin();
    void requestPluginAuthToken(const QString accout, const QString token);
    void requestUpdateBlurEffectGeometry();
    void passwordErrorTipsClearChanged(const bool isClear);
    void requestSendExtraInfo(const QString &info);

public slots:
    void setResetPasswordMessageVisible(const bool isVisible, bool fromResetDialog = false);
    void updateResetPasswordUI();
    void onReadyToAuthChanged(bool ready);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void initConnections();
    void updateUnlockPrompt() override;
    void showPasswordHint();
    void setPasswordHintBtnVisible(const bool isVisible);
    bool isUserAccountBinded();
    void showAlertMessage(const QString &text);
    void hidePasswordHintWidget();
    void updatePasswordTextMargins();
    QString getCurrentKBLayout() const;
    void setKBLayout(const QString &layout);

private:
    bool m_passwordLineEditEnabled;
    DLabel *m_capsLock;             // 大小写状态
    DLineEditEx *m_lineEdit;        // 密码输入框
    DIconButton *m_passwordShowBtn; // 密码显示按钮
    DIconButton *m_passwordHintBtn; // 密码提示按钮
    PasswordErrorTipsWidget* m_passwordTipsWidget;  // 认证错误提示窗口
    QString m_passwordHint;         // 密码提示
    bool m_resetPasswordMessageVisible;
    DFloatingMessage *m_resetPasswordFloatingMessage;
    uid_t m_currentUid; // 当前用户uid
    QTimer *m_bindCheckTimer;
    DAlertControl *m_passwordHintWidget;
    DIconButton *m_iconButton;
    bool m_resetDialogShow;

    bool m_isPasswdAuthWidgetReplaced;
    AssistLoginWidget *m_assistLoginWidget;
    DConfig *m_authenticationDconfig;
    bool m_canShowPasswordErrorTips;
    QString m_originalKBLayout;
};

#endif // AUTHPASSWORD_H
