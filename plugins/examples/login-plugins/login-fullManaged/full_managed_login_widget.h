// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __FULL_MANAGED_LOGIN_WIDGET__H__
#define __FULL_MANAGED_LOGIN_WIDGET__H__

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

// when this plugin example set to deactive, it would automaticaly reset active after time interval below
#define DEACTIVE_TIME_INTERVAL 30000

class QLabel;

class FullManagedLoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FullManagedLoginWidget(QWidget *parent = nullptr);

    void showMessage(const QString &message);
    void hideMessage();

private:
    void initUI();
    void initConnection();

Q_SIGNALS:
    void sendAuthToken(const QString &account, const QString &token);
    void reset();
    void deactived();
    void greeterAuthStarted();

private:
    QLabel *m_messageLabel;
    QLabel *m_userNameLabel;
    QLineEdit *m_userNameEdit;
    QLabel *m_tokenLabel;
    QLineEdit *m_tokenEdit;
    QPushButton *m_sendButton;
    QPushButton *m_disMissButton;
};

#endif //!__FULL_MANAGED_LOGIN_WIDGET__H__
