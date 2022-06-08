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

#ifndef AUTHPASSWORD_H
#define AUTHPASSWORD_H

#include "auth_module.h"

#include <DIconButton>
#include <DLabel>
#include <DPushButton>
#include <DFloatingMessage>
#include <DSuggestButton>
#include <DMessageManager>

#define Password_Auth QStringLiteral(":/misc/images/auth/password.svg")
//const QString Password_Auth = ":/misc/images/auth/password.svg";

class DLineEditEx;
DWIDGET_USE_NAMESPACE

class AuthPassword : public AuthModule
{
    Q_OBJECT
public:
    explicit AuthPassword(QWidget *parent = nullptr);

    void reset();
    QString lineEditText() const;

    void setAnimationState(const bool start) override;
    void setAuthState(const int state, const QString &result) override;
    void setCapsLockVisible(const bool on);
    void setLimitsInfo(const LimitsInfo &info) override;
    void setLineEditEnabled(const bool enable);
    void setLineEditInfo(const QString &text, const TextType type);
    void setPasswordHint(const QString &hint);
    void setCurrentUid(uid_t uid);
    void showResetPasswordMessage();
    void closeResetPasswordMessage();
    void setAuthStatueVisible(bool visible);

signals:
    void focusChanged(const bool);
    void lineEditTextChanged(const QString &); // 数据同步
    void requestChangeFocus();                 // 切换焦点
    void requestShowKeyboardList();            // 显示键盘布局列表
    void resetPasswordMessageVisibleChanged(const bool isVisible);

public slots:
    void setResetPasswordMessageVisible(const bool isVisible);
    void updateResetPasswordUI();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void initUI();
    void initConnections();
    void updateUnlockPrompt() override;
    void showPasswordHint();
    void setPasswordHintBtnVisible(const bool isVisible);
    bool isUserAccountBinded();

private:
    DLabel *m_capsLock;             // 大小写状态
    DLineEditEx *m_lineEdit;        // 密码输入框
    DIconButton *m_passwordHintBtn; // 密码提示按钮
    QString m_passwordHint;         // 密码提示
    bool m_resetPasswordMessageVisible;
    DFloatingMessage *m_resetPasswordFloatingMessage;
    uid_t m_currentUid; // 当前用户uid
    QTimer *m_bindCheckTimer;
};

#endif // AUTHPASSWORD_H
