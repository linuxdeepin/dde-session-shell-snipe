// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef USE_DEEPIN_WAYLAND
#include "keyboardplantform_wayland.h"
#include "constants.h"

#include <linux/input-event-codes.h>

#include <KF5/KWayland/Client/registry.h>
#include <KF5/KWayland/Client/event_queue.h>
#include <KF5/KWayland/Client/connection_thread.h>
#include <KF5/KWayland/Client/ddeseat.h>
#include <KF5/KWayland/Client/ddekeyboard.h>
#include <KF5/KWayland/Client/fakeinput.h>

#include <QThread>
#include <QDebug>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QApplication>
#include <QProcess>

KeyboardPlatformWayland::KeyboardPlatformWayland(QObject *parent)
    : KeyBoardPlatform(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
    , m_ddeKeyboard(nullptr)
    , m_fakeInput(nullptr)
    , m_ddeSeat(nullptr)
    , m_eventQueue(nullptr)
    , m_capsLock(false)
    , m_numLockOn(false)
    , m_repeatTimer(new QTimer(this))
{
    initStatus();

    m_repeatTimer->setInterval(100);
    m_repeatTimer->setSingleShot(true);
}

KeyboardPlatformWayland::~KeyboardPlatformWayland()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

bool KeyboardPlatformWayland::isCapsLockOn()
{
    return m_capsLock;
}

bool KeyboardPlatformWayland::isNumLockOn()
{
    return m_numLockOn;
}

bool KeyboardPlatformWayland::setNumLockStatus(const bool &on)
{
    qDebug() << "Set num lock state: " << on << ", numlockon: " << m_numLockOn;
    if (m_fakeInput && m_fakeInput->isValid() && on != m_numLockOn) {
        m_fakeInput->authenticate(QStringLiteral("lightdm-deepin-greeter"), QStringLiteral("Set num lock state"));
        m_fakeInput->requestKeyboardKeyPress(KEY_NUMLOCK);
        m_fakeInput->requestKeyboardKeyRelease(KEY_NUMLOCK);
        m_repeatTimer->start();
        m_numLockOn = !m_numLockOn;
        return true;
    }
    qDebug() << "Fake input is not ready";
    return false;
}

void KeyboardPlatformWayland::run()
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

void KeyboardPlatformWayland::setupRegistry(Registry *registry)
{
    qCInfo(DDE_SHELL) << "Set up registry: " << registry;
    connect(registry, &Registry::ddeSeatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_ddeSeat = registry->createDDESeat(name, version, this);
        if (m_ddeSeat) {
            // 先断开连接在重新连接，防止多次创建之后，导致重复连接
            disconnect(m_ddeKeyboard, &DDEKeyboard::keyChanged, this, nullptr);
            m_ddeKeyboard = m_ddeSeat->createDDEKeyboard(this);
            connect(m_ddeKeyboard, &DDEKeyboard::keyChanged, this, [this](quint32 key, KWayland::Client::DDEKeyboard::KeyState state, quint32 time) {
                if (state == KWayland::Client::DDEKeyboard::KeyState::Pressed) {
                    if (key == KEY_CAPSLOCK) {
                        m_capsLock = !m_capsLock;
                        qDebug() << "CapsLock state: " << m_capsLock;
                        Q_EMIT capsLockStatusChanged(m_capsLock);
                    } else if (key == KEY_NUMLOCK) {
                        if (m_repeatTimer->isActive())
                            return;

                        m_numLockOn = !m_numLockOn;
                        qDebug() << "NumLock state: " << m_numLockOn;
                        Q_EMIT numLockStatusChanged(m_numLockOn);
                    }
                }
            });
        }
    });

    connect(registry, &Registry::fakeInputAnnounced, this, [ this, registry ](quint32 name, quint32 version) {
        qDebug() << "Create fakeinput.";
        m_fakeInput = registry->createFakeInput(name, version, this);
        if (!m_fakeInput->isValid()) {
            qCWarning(DDE_SHELL) << "Create fakeinput failed.";
            return;
        }

        Q_EMIT initialized();
    });

    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

// 对于dde-lock需要初始化大小写状态
void KeyboardPlatformWayland::initStatus()
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.KWin", "/Xkb", "org.kde.kwin.Xkb", "getLeds");
    QDBusPendingCall async = QDBusConnection::sessionBus().asyncCall(message);
    auto *watcher = new QDBusPendingCallWatcher(async);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *callWatcher) {
        QDBusPendingReply<void> reply = *callWatcher;
        if (!reply.isError()) {
            int locked = reply.argumentAt(0).toInt();
            m_capsLock = (locked & 0x02) != 0;
            // 异步结果通过信号发布
            Q_EMIT capsLockStatusChanged(m_capsLock);
            qCDebug(DDE_SHELL) << "capslock status init : " << m_capsLock;

            // greeter中numlock状态来自配置
            if (qApp && qApp->applicationName().contains("lock")) {
                m_numLockOn = (locked & 0x10) != 0;
                Q_EMIT numLockStatusChanged(m_numLockOn);
                qCDebug(DDE_SHELL) << "numlock status init : " << m_numLockOn;
            }
        } else {
            qCWarning(DDE_SHELL) << "failed to init caps/num lock status :" << reply.error().message();
        }

        callWatcher->deleteLater();
        watcher->deleteLater();
    });
}

void KeyboardPlatformWayland::ungrabKeyboard()
{
    QProcess::execute("bash -c \"originmap=$(setxkbmap -query | grep option | awk -F ' ' '{print $2}');/usr/bin/setxkbmap -option grab:break_actions&&/usr/bin/xdotool key XF86Ungrab&&setxkbmap -option $originmap\"");
}

#endif
