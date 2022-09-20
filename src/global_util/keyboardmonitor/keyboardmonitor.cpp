// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardmonitor.h"
#include <DGuiApplicationHelper>

DGUI_USE_NAMESPACE

KeyboardMonitor::KeyboardMonitor()
    : QThread()
    , m_keyBoardPlatform(nullptr)
{
    if (DGuiApplicationHelper::isXWindowPlatform()) {
        m_keyBoardPlatform = new KeyboardPlantformX11();
    } else {
#ifdef USE_DEEPIN_WAYLAND
        m_keyBoardPlatform = new KeyboardPlantformWayland();
#endif
    }

    if (m_keyBoardPlatform) {
        connect(m_keyBoardPlatform, &KeyBoardPlatform::capslockStatusChanged, this, &KeyboardMonitor::capslockStatusChanged);
        connect(m_keyBoardPlatform, &KeyBoardPlatform::numlockStatusChanged, this, &KeyboardMonitor::numlockStatusChanged);
        connect(m_keyBoardPlatform, &KeyBoardPlatform::initialized, this, &KeyboardMonitor::initialized);
    }
}

KeyboardMonitor *KeyboardMonitor::instance()
{
    static KeyboardMonitor *KeyboardMonitorInstance = nullptr;

    if (!KeyboardMonitorInstance) {
        KeyboardMonitorInstance = new KeyboardMonitor;
    }

    return KeyboardMonitorInstance;
}

bool KeyboardMonitor::isCapslockOn()
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->isCapslockOn();
}

bool KeyboardMonitor::isNumlockOn()
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->isNumlockOn();
}

bool KeyboardMonitor::setNumlockStatus(const bool &on)
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->setNumlockStatus(on);
}

void KeyboardMonitor::run()
{
    if (!m_keyBoardPlatform)
        return;

    m_keyBoardPlatform->run();
}
