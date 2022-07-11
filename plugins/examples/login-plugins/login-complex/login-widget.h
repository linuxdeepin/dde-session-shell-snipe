/*
* Copyright (C) 2021 ~ 2022 Uniontech Software Technology Co.,Ltd.
*
* Author:     YinJie <yinjie@uniontech.com>
*
* Maintainer: YinJie <yinjie@uniontech.com>
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