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
