// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTIVESESSIONWIDGETS_H
#define ACTIVESESSIONWIDGETS_H

#include <QWidget>

#include <memory>

namespace Dtk {
namespace Widget {
class DSpinner;
}
}

class QLabel;
class StateButton;
class UserAvatar;
class User;

class ActiveSessionConfirmWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActiveSessionConfirmWidget(QWidget *parent = nullptr);
    void setUser(const std::shared_ptr<User> &user);
    void setLimitMode(bool limitMode);

signals:
    void confirmed();
    void canceled();

private:
    void initUI();
    void updateAvatar();

private:
    UserAvatar *m_avatar;
    QLabel *m_userNameLabel;
    QLabel *m_messageLabel;
    StateButton *m_confirmButton;
    StateButton *m_cancelButton;
    std::shared_ptr<User> m_user;
};

class ActiveSessionLoadingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActiveSessionLoadingWidget(QWidget *parent = nullptr);

private:
    void initUI();

private:
    Dtk::Widget::DSpinner *m_spinner;
};

#endif  // ACTIVESESSIONWIDGETS_H
