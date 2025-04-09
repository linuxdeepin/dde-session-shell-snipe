// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __LOGIN_WIDGET__H__
#define __LOGIN_WIDGET__H__

#include <QWidget>

class QLabel;

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr);

    void showMessage(const QString &message);
    void hideMessage();

private:
    void init();

Q_SIGNALS:
    void sendAuthToken(const QString &account, const QString &token);
    void reset();

private:
    QLabel *m_messageLabel;
};

#endif  //!__LOGIN_WIDGET__H__