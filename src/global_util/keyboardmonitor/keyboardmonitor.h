// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYBOARDMONITOR_H
#define KEYBOARDMONITOR_H

#include <QThread>
#include "keyboardplantform_x11.h"
#ifdef USE_DEEPIN_WAYLAND
#include "keyboardplantform_wayland.h"
#endif

#define DPMS_STATE_FILE "/tmp/dpms-state" //black screen state;bug:222049

class KeyboardMonitor : public QThread
{
    Q_OBJECT
public:
    static KeyboardMonitor *instance();

    bool isCapsLockOn();
    bool isNumLockOn();
    bool setNumLockStatus(const bool &on);
    void ungrabKeyboard();

signals:
    void capsLockStatusChanged(bool on);
    void numLockStatusChanged(bool on);
    void initialized();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    KeyboardMonitor();
    KeyBoardPlatform* m_keyBoardPlatform;
};

#endif // KEYBOARDMONITOR_H
