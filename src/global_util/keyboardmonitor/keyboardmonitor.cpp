// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardmonitor.h"

#include <DGuiApplicationHelper>

#include <QFile>

DGUI_USE_NAMESPACE

KeyboardMonitor::KeyboardMonitor()
    : QThread()
    , m_keyBoardPlatform(nullptr)
{
    if (DGuiApplicationHelper::isXWindowPlatform()) {
        m_keyBoardPlatform = new KeyboardPlatformX11();
    } else {
#ifdef USE_DEEPIN_WAYLAND
        m_keyBoardPlatform = new KeyboardPlatformWayland();
#endif
    }

    if (m_keyBoardPlatform) {
        connect(m_keyBoardPlatform, &KeyBoardPlatform::capsLockStatusChanged, this, &KeyboardMonitor::capsLockStatusChanged);
        connect(m_keyBoardPlatform, &KeyBoardPlatform::numLockStatusChanged, this, &KeyboardMonitor::numLockStatusChanged);
        connect(m_keyBoardPlatform, &KeyBoardPlatform::initialized, this, &KeyboardMonitor::initialized);
    }

    start(QThread::LowestPriority);
}

KeyboardMonitor *KeyboardMonitor::instance()
{
    static KeyboardMonitor *KeyboardMonitorInstance = nullptr;

    if (!KeyboardMonitorInstance) {
        KeyboardMonitorInstance = new KeyboardMonitor;
    }

    return KeyboardMonitorInstance;
}

bool KeyboardMonitor::isCapsLockOn()
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->isCapsLockOn();
}

bool KeyboardMonitor::isNumLockOn()
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->isNumLockOn();
}

bool KeyboardMonitor::setNumLockStatus(const bool &on)
{
    if (!m_keyBoardPlatform)
        return false;

    return m_keyBoardPlatform->setNumLockStatus(on);
}

void KeyboardMonitor::run()
{
    if (!m_keyBoardPlatform)
        return;

    m_keyBoardPlatform->run();
}

void KeyboardMonitor::ungrabKeyboard()
{
    if (!m_keyBoardPlatform)
        return;

    if (!(QFile::exists(DPMS_STATE_FILE) && QFile(DPMS_STATE_FILE).readAll() == "1")) {
        m_keyBoardPlatform->ungrabKeyboard();
    }
}