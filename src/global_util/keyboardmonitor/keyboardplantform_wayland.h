// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYBOARDPLANTFORM_WAYLAND_H
#define KEYBOARDPLANTFORM_WAYLAND_H

#ifdef USE_DEEPIN_WAYLAND

#include "keyboardplatform.h"

#include <QTimer>

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
class KeyboardPlatformWayland : public KeyBoardPlatform
{
    Q_OBJECT

public:
    KeyboardPlatformWayland(QObject *parent = nullptr);
    virtual ~KeyboardPlatformWayland();

    bool isCapsLockOn() override;
    bool isNumLockOn() override;
    bool setNumLockStatus(const bool &on) override;
    void ungrabKeyboard() override;

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void setupRegistry(Registry *registry);
    void initStatus();

private:
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    DDEKeyboard *m_ddeKeyboard;
    FakeInput *m_fakeInput;
    DDESeat *m_ddeSeat;
    EventQueue *m_eventQueue;
    bool m_capsLock;
    bool m_numLockOn;
    QTimer *m_repeatTimer;
};

#endif

#endif // KEYBOARDPLANTFORM_WAYLAND_H
