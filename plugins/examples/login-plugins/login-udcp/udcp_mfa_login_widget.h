// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later


#ifndef UDCP_MFA_LOGIN_WIDGET_H
#define UDCP_MFA_LOGIN_WIDGET_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class UdcpMFALoginWidget: public QWidget
{
    Q_OBJECT
public:
    explicit UdcpMFALoginWidget(const QString &user, QWidget *parent = Q_NULLPTR);
    void reset();

Q_SIGNALS:
    void finished();

private:
    void initUI();
    void initConnections();

    int m_count {}; // 倒计时
    const QString m_user {};

    QTimer *m_timer {};
    QLineEdit *m_phoneEdit {};
    QPushButton *m_sendButton {};
    QLineEdit *m_codeEdit {};
    QPushButton *m_verifyButton {};
};

#endif // UDCP_MFA_LOGIN_WIDGET_H
