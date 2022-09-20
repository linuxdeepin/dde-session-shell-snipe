// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
