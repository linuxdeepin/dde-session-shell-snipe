/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmai.com>
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

#ifndef KEYBOARDPLATFORM_H
#define KEYBOARDPLATFORM_H

#include <QObject>

class KeyBoardPlatform : public QObject
{
    Q_OBJECT
public:
    KeyBoardPlatform(QObject *parent = nullptr);
    virtual ~KeyBoardPlatform();

    virtual bool isCapslockOn() = 0;
    virtual bool isNumlockOn() = 0;
    virtual bool setNumlockStatus(const bool &on) = 0;
    virtual void run() = 0;

signals:
    void capslockStatusChanged(bool on);
    void numlockStatusChanged(bool on);
    void initialized();
};

#endif // KEYBOARDPLATFORM_H
