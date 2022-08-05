/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *             zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *             zorowk <near.kingzero@gmail.com>
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

#ifndef KEYBOARDMONITOR_H
#define KEYBOARDMONITOR_H

#include <QThread>
#include "keyboardplantform_x11.h"
#ifdef USE_DEEPIN_WAYLAND
#include "keyboardplantform_wayland.h"
#endif

class KeyboardMonitor : public QThread
{
    Q_OBJECT
public:
    static KeyboardMonitor *instance();

    bool isCapslockOn();
    bool isNumlockOn();
    bool setNumlockStatus(const bool &on);

signals:
    void capslockStatusChanged(bool on);
    void numlockStatusChanged(bool on);
    void initialized();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    KeyboardMonitor();
    KeyBoardPlatform* keyBoardPlatform = nullptr;
};

#endif // KEYBOARDMONITOR_H
