// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINTIPSWINDOW_H
#define LOGINTIPSWINDOW_H

#include "fullscreenbackground.h"
#include "public_func.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QHBoxLayout;
class QPalette;

class LoginTipsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginTipsWindow(QWidget *parent = 0);
    bool isValid();

signals:
    void requestClosed();

private:
    void initUI();

private:
    QHBoxLayout *m_mainLayout;
    QLabel *m_tipLabel;
    QLabel *m_content;
    QPushButton *m_btn;
    QString m_tipString;
    QString m_contentString;

};

#endif // LOGINTIPSWINDOW_H
