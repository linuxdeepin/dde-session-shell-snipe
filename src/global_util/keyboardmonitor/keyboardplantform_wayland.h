/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmail.com>
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

#ifndef KEYBOARDPLANTFORM_WAYLAND_H
#define KEYBOARDPLANTFORM_WAYLAND_H

#ifdef USE_DEEPIN_WAYLAND

#include "keyboardplatform.h"

class QThread;

namespace KWayland
{
namespace Client
{
    class Registry;
    class ConnectionThread;
    class DDEKeyboard;
    class DDESeat;
    class EventQueue;
    class FakeInput;
}
}
using namespace KWayland::Client;
class KeyboardPlantformWayland : public KeyBoardPlatform
{
    Q_OBJECT

public:
    KeyboardPlantformWayland(QObject *parent = nullptr);
    virtual ~KeyboardPlantformWayland();

    bool isCapslockOn() override;
    bool isNumlockOn() override;
    bool setNumlockStatus(const bool &on) override;

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void setupRegistry(Registry *registry);

private:
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    DDEKeyboard *m_ddeKeyboard;
    FakeInput *m_fakeInput;
    DDESeat *m_ddeSeat;
    EventQueue *m_eventQueue;
    bool m_capsLock;
    bool m_numLockOn;
};

#endif

#endif // KEYBOARDPLANTFORM_WAYLAND_H
