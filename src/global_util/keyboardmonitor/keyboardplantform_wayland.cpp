/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include "keyboardplantform_wayland.h"

#include <stdio.h>
#include <stdlib.h>

#include <KF5/KWayland/Client/registry.h>
#include <KF5/KWayland/Client/event_queue.h>
#include <KF5/KWayland/Client/connection_thread.h>
#include <KF5/KWayland/Client/ddeseat.h>
#include <KF5/KWayland/Client/ddekeyboard.h>

#include <QThread>
#include <QDebug>

const quint32 Key_CapsLock = 58;

KeyboardPlantformWayland::KeyboardPlantformWayland(QObject *parent)
    : KeyBoardPlatform(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
    , m_ddeKeyboard(nullptr)
    , m_ddeSeat(nullptr)
    , m_eventQueue(nullptr)
    , m_capsLock(false)
{

}

KeyboardPlantformWayland::~KeyboardPlantformWayland()
 {
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
 }

bool KeyboardPlantformWayland::isCapslockOn()
{
    return m_capsLock;
}

bool KeyboardPlantformWayland::isNumlockOn()
{
    return false;
}

bool KeyboardPlantformWayland::setNumlockStatus(const bool &on)
{
    Q_UNUSED(on);
    return false;
}

void KeyboardPlantformWayland::run()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this, [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);
            Registry *registry = new Registry(this);
            setupRegistry(registry);
        }, Qt::QueuedConnection
    );
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();
    m_connectionThreadObject->initConnection();
}

void KeyboardPlantformWayland::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::ddeSeatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_ddeSeat = registry->createDDESeat(name, version, this);
        if (m_ddeSeat) {
            m_ddeKeyboard = m_ddeSeat->createDDEKeyboard(this);
            connect(m_ddeKeyboard, &DDEKeyboard::keyChanged, this, [this] (quint32 key, KWayland::Client::DDEKeyboard::KeyState state, quint32 time) {
                if (key == Key_CapsLock && int(state) == 1) {
                    m_capsLock = !m_capsLock;
                    qDebug() << "m_capsLock：" << m_capsLock;
                    Q_EMIT capslockStatusChanged(m_capsLock);
                }
            });
        }
    });
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}
