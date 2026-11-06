// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DLineEdit>
#include "password_input_widget.h"
#include "inhibitbutton.h"
#include "securitylevelitem.h"
#include <DBlurEffectWidget>
#include <DLabel>
#include <DPushButton>
#include <DSuggestButton>

#include <QWidget>
#include <QPlainTextEdit>
#include <memory>

class User;
class UserAvatar;
class UserNameWidget;

// Shared lock/greeter presentation; each worker owns its authentication backend.
class LockPasswordChangeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LockPasswordChangeWidget(QWidget *parent = nullptr);
    void setUser(const std::shared_ptr<User> &user);
    void setState(bool currentRequired, bool checkingOnly, bool busy, const QString &error);
    void clearInputs();

signals:
    void requestChangePassword(const QString &current, const QString &password,
                               const QString &confirmation, const QString &hint);
    void requestCancel();
    void userIsActive();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void submit();
    void focusInput();
    bool validateInputs();
    QString localPasswordError(const QString &password) const;
    void clearAlerts();
    void showFieldError(Dtk::Widget::DLineEdit *edit, const QString &error);
    void updateBlurEffectGeometry();
    UserAvatar *m_avatar;
    UserNameWidget *m_userName;
    Dtk::Widget::DLabel *m_info;
    QPlainTextEdit *m_error;
    PasswordInputWidget *m_current;
    PasswordInputWidget *m_password;
    PasswordInputWidget *m_confirmation;
    Dtk::Widget::DLineEdit *m_hint;
    InhibitButton *m_submit;
    InhibitButton *m_cancel;
    Dtk::Widget::DBlurEffectWidget *m_blurEffectWidget;
    dcc::widgets::SecurityLevelItem *m_strength;
    Dtk::Widget::DLabel *m_warning;
    QString m_accountName;
    QString m_fullName;
    bool m_checkingOnly = false;
    bool m_currentRequired = false;
    bool m_busy = false;
    bool m_localSecurityCheck = true;
};
