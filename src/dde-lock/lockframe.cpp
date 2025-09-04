// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockframe.h"

#include "lockcontent.h"
#include "sessionbasemodel.h"
#include "userinfo.h"
#include "warningcontent.h"
#include "public_func.h"
#include "dconfig_helper.h"

#include <DPlatformWindowHandle>

#include <QApplication>
#include <QDBusInterface>

#include "dbusconstant.h"
#ifndef ENABLE_DSS_SNIPE
#include <QGSettings>
#include <QX11Info>
#else
#include <xcb/xproto.h>
#endif

#define LOCK_START_EFFECT 16 //锁屏动效比较特殊，窗管根据这个动效值进行处理

xcb_atom_t internAtom(xcb_connection_t *connection, const char *name, bool only_if_exists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, only_if_exists, static_cast<uint16_t>(strlen(name)), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}

LockFrame::LockFrame(SessionBaseModel *const model, QWidget *parent)
    : FullScreenBackground(model, parent)
    , m_model(model)
    , m_enablePowerOffKey(false)
    , m_autoExitTimer(nullptr)
{
#ifndef ENABLE_DSS_SNIPE
    xcb_connection_t *connection = QX11Info::connection();
#else
    xcb_connection_t *connection = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
#endif
    if (connection) {
        xcb_atom_t cook = internAtom(connection, "_DEEPIN_LOCK_SCREEN", false);
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, static_cast<xcb_window_t>(winId()),
                            cook, XCB_ATOM_ATOM, 32, 1, &cook);

        // x11下通过窗口属性设置锁屏启动动效
        xcb_atom_t startup = internAtom(connection, "_DEEPIN_NET_STARTUP", false);
        quint32 value = LOCK_START_EFFECT;
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, static_cast<xcb_window_t>(winId()),
                            startup, XCB_ATOM_CARDINAL, 32, 1, &value);
    }

    setAccessibleName("LockFrame");

    // wayland下通过DTK设置锁屏启动动效
    if (qgetenv("XDG_SESSION_TYPE").contains("wayland")) {
        DPlatformWindowHandle handle(this, this);
        handle.setWindowStartUpEffect(static_cast<DPlatformWindowHandle::EffectType>(LOCK_START_EFFECT));
    }

    if (!currentContent) {
        setContent(LockContent::instance());
        m_model->setCurrentContentType(SessionBaseModel::LockContent);
    }
    connect(LockContent::instance(), &LockContent::requestBackground, this, static_cast<void (LockFrame::*)(const QString &)>(&LockFrame::updateBackground));
    connect(model, &SessionBaseModel::prepareForSleep, this, &LockFrame::prepareForSleep);
    connect(model, &SessionBaseModel::authFinished, this, [this](bool success) {
        if (success) {
            Q_EMIT requestEnableHotzone(true);
            if (!WarningContent::instance()->supportDelayOrWait())
                m_model->setVisible(false);
        }
    });

    //程序启动1秒后才响应电源按键信号，避免第一次待机唤醒启动锁屏程序后响应信号，将界面切换到关机选项
    QTimer::singleShot(1000, this, [this] {
        m_enablePowerOffKey = true;
    });

    if (DConfigHelper::instance()->getConfig("autoExit", false).toBool()) {
        m_autoExitTimer = new QTimer(this);
        m_autoExitTimer->setInterval(1000 * 60); //1分钟
        m_autoExitTimer->setSingleShot(true);
        connect(m_autoExitTimer, &QTimer::timeout, qApp, &QApplication::quit);
    }
}

LockFrame::~LockFrame()
{
    if (LockContent::instance()->parent() == this) {
        LockContent::instance()->setParent(nullptr);
    }

    if (WarningContent::instance()->parent() == this) {
        WarningContent::instance()->setParent(nullptr);
    }
}

bool LockFrame::event(QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QString keyValue = "";
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_PowerOff: {
            if (!handlePoweroffKey()) {
                keyValue = "power-off";
            }
            break;
        }
        case Qt::Key_NumLock: {
            keyValue = "numlock";
            break;
        }
        case Qt::Key_TouchpadOn: {
            keyValue = "touchpad-on";
            break;
        }
        case Qt::Key_TouchpadOff: {
            keyValue = "touchpad-off";
            break;
        }
        case Qt::Key_TouchpadToggle: {
            keyValue = "touchpad-toggle";
            break;
        }
        case Qt::Key_CapsLock: {
            keyValue = "capslock";
            break;
        }
        case Qt::Key_VolumeDown: {
            keyValue = "audio-lower-volume";
            break;
        }
        case Qt::Key_VolumeUp: {
            keyValue = "audio-raise-volume";
            break;
        }
        case Qt::Key_VolumeMute: {
            keyValue = "audio-mute";
            break;
        }
        case Qt::Key_MonBrightnessUp: {
            keyValue = "mon-brightness-up";
            break;
        }
        case Qt::Key_MonBrightnessDown: {
            keyValue = "mon-brightness-down";
            break;
        }
        // 锁屏时会独占键盘，需要单独处理F1待机事件
        case Qt::Key_Sleep: {
            m_model->setPowerAction(SessionBaseModel::RequireSuspend);
            break;
        }
        }

        if (keyValue != "") {
            emit sendKeyValue(keyValue);
        }
    }
    return FullScreenBackground::event(event);
}

void LockFrame::resizeEvent(QResizeEvent *event)
{
    if (contentVisible()) {
        LockContent::instance()->resize(size());
    }
    FullScreenBackground::resizeEvent(event);
}

bool LockFrame::handlePoweroffKey()
{
    qCInfo(DDE_SHELL) << "Handle poweroff key";
    // 升级的时候，不响应电源按钮
    if (m_model->currentContentType() == SessionBaseModel::UpdateContent) {
        qCInfo(DDE_SHELL) << "Is updating, do not handle power off key";
        return false;
    }

    QDBusInterface powerInter(DSS_DBUS::sessionPowerService, DSS_DBUS::sessionPowerPath,DSS_DBUS::sessionPowerService);
    if (!powerInter.isValid()) {
        qCWarning(DDE_SHELL) << "Poweroff interface is not valid";
        return false;
    }
    bool isBattery = powerInter.property("OnBattery").toBool();
    int action = 0;
    if (isBattery) {
        action = powerInter.property("BatteryPressPowerBtnAction").toInt();
    } else {
        action = powerInter.property("LinePowerPressPowerBtnAction").toInt();
    }
    qCDebug(DDE_SHELL) << "Whether on battery: " << isBattery << ", press poweroff button action: " << action;
    // 需要特殊处理：关机(0)和无任何操作(4)
    if (action == 0) {
        // 待机刚唤醒一段时间内不响应电源按键事件
        if (m_enablePowerOffKey) {
            //锁屏时或显示关机界面时，需要确认是否关机
            emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, false);
        }
        return true;
    } else if (action == 4) {
        // 先检查当前是否允许响应电源按键
        if (m_enablePowerOffKey && m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode) {
            //无任何操作时，如果是锁定时显示小关机界面
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
        }
        return true;
    }
    return false;
}

void LockFrame::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:
        qApp->quit();
        break;
#endif
    }
}

void LockFrame::showEvent(QShowEvent *event)
{
    emit requestEnableHotzone(false);

    if (m_autoExitTimer)
        m_autoExitTimer->stop();

    return FullScreenBackground::showEvent(event);
}

void LockFrame::hideEvent(QHideEvent *event)
{
    emit requestEnableHotzone(true);

    if (m_autoExitTimer)
        m_autoExitTimer->start();

    return FullScreenBackground::hideEvent(event);
}

void LockFrame::prepareForSleep(bool isSleep)
{
    //不管是待机还是唤醒均不响应电源按键信号
    m_enablePowerOffKey = false;
    // 唤醒不需要密码时，在性能差的机器上可能会在1秒后才收到电源事件，依就会响应电源事件
    // 或者将延时改的更长
    //唤醒时间改为3秒后才响应电源按键信号，避免待机唤醒时响应信号，将界面切换到关机选项
    if (!isSleep) {
        QTimer::singleShot(3000, this, [this] {
            m_enablePowerOffKey = true;
        });
    }
}
