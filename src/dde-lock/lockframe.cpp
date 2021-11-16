/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
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

#include "lockframe.h"
#include "lockcontent.h"
#include "sessionbasemodel.h"
#include "userinfo.h"
#include "warningcontent.h"

#include <QApplication>
#include <QWindow>
#include <QDBusInterface>
#include <QScreen>
#include <QGSettings>

LockFrame::LockFrame(SessionBaseModel *const model, QWidget *parent)
    : FullscreenBackground(parent)
    , m_model(model)
    , m_lockContent(new LockContent(model))
    , m_warningContent(nullptr)
    , m_preparingSleep(false)
    , m_prePreparingSleep(false)
{
    qDebug() << "LockFrame geometry:" << geometry();

    QTimer::singleShot(0, this, [ = ] {
        auto user = model->currentUser();
        if (user != nullptr) {
            //默认刷新清晰的锁屏背景，避免因为获取虚化背景过慢而引起的白屏问题
            updateBackground(QPixmap(user->greeterBackgroundPath()));
        }
    });

    m_lockContent->hide();
    setContent(m_lockContent);

    connect(m_lockContent, &LockContent::requestSetLocked, this, &LockFrame::requestSetLocked);
    connect(m_lockContent, &LockContent::requestSwitchToUser, this, &LockFrame::requestSwitchToUser);
    connect(m_lockContent, &LockContent::requestSetLayout, this, &LockFrame::requestSetLayout);
    connect(m_lockContent, &LockContent::requestBackground, this, static_cast<void (LockFrame::*)(const QString &)>(&LockFrame::updateBackground));
    connect(m_lockContent, &LockContent::requestStartAuthentication, this, &LockFrame::requestStartAuthentication);
    connect(m_lockContent, &LockContent::sendTokenToAuth, this, &LockFrame::sendTokenToAuth);
    connect(model, &SessionBaseModel::blackModeChanged, this, &FullscreenBackground::setIsBlackMode);
    connect(model, &SessionBaseModel::showUserList, this, &LockFrame::showUserList);
    connect(model, &SessionBaseModel::showLockScreen, this, &LockFrame::showLockScreen);
    connect(model, &SessionBaseModel::showShutdown, this, &LockFrame::showShutdown);
    connect(model, &SessionBaseModel::shutdownInhibit, this, &LockFrame::shutdownInhibit);
    connect(model, &SessionBaseModel::cancelShutdownInhibit, this, &LockFrame::cancelShutdownInhibit);
    connect(model, &SessionBaseModel::prepareForSleep, this, [ = ](bool isSleep) {
        //初始化时，m_prePreparingSleep = false,m_preparingSleep = false
        //开始待机时，isSleep为true,那么m_prePreparingSleep = false,m_preparingSleep = true
        //待机唤醒后，isSleep为false,那么m_prePreparingSleep = true,m_preparingSleep = false
        m_prePreparingSleep = m_preparingSleep;
        m_preparingSleep = isSleep;
        //记录待机和唤醒时间
        m_preparingSleepTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        //待机时由锁屏提供假黑屏，唤醒时显示正常界面
        model->setVisible(true);
        model->setIsBlackModel(isSleep);

        if (!isSleep) {
            //待机唤醒后检查是否需要密码，若不需要密码直接隐藏锁定界面
            if (QGSettings::isSchemaInstalled("com.deepin.dde.power")) {
                QGSettings powerSettings("com.deepin.dde.power", QByteArray(), this);
                if (!powerSettings.get("sleep-lock").toBool()) {
                    hide();
                }
            }
        }
    } );

    connect(model, &SessionBaseModel::authFinished, this, [ = ](bool success){
        m_lockContent->beforeUnlockAction(success);

        if (success) {
            Q_EMIT requestEnableHotzone(true);
            hide();
        }
    });
}

bool LockFrame::event(QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QString  keyValue = "";
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
        }

        if (keyValue != "") {
            emit sendKeyValue(keyValue);
        }
    }
    return FullscreenBackground::event(event);
}

bool LockFrame::handlePoweroffKey()
{
    QDBusInterface powerInter("com.deepin.daemon.Power","/com/deepin/daemon/Power","com.deepin.daemon.Power");
    if (!powerInter.isValid()) {
        qDebug() << "powerInter is not valid";
        return false;
    }
    bool isBattery = powerInter.property("OnBattery").toBool();
    int action = 0;
    if (isBattery) {
        action = powerInter.property("BatteryPressPowerBtnAction").toInt();
    } else {
        action = powerInter.property("LinePowerPressPowerBtnAction").toInt();
    }
    qDebug() << "battery is: " << isBattery << "," << action;
    // 需要特殊处理：关机(0)和无任何操作(4)
    if (action == 0) {
        //锁屏时或显示关机界面时，需要确认是否关机
        emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, false);
        return true;
    } else if (action == 4) {
        //初始化时，m_prePreparingSleep = false,m_preparingSleep = false
        //开始待机时，isSleep为true,那么m_prePreparingSleep = false,m_preparingSleep = true
        //待机唤醒后，isSleep为false,那么m_prePreparingSleep = true,m_preparingSleep = false

        // 先检查当前是不是准备待机
        if (!m_preparingSleep) {
            //有些机器使用电源唤醒时，除了会唤醒机器外还会发送按键消息，会将锁屏界面切换成电源选项界面,增加唤醒时500毫秒时间检测
            //如果系统刚唤醒 ，则500毫秒内不响应电源按钮事件
            if (m_prePreparingSleep && QDateTime::currentDateTime().toMSecsSinceEpoch() - m_preparingSleepTime < 500) {
                return true;
            }
            //无任何操作时，如果是锁定时显示小关机界面，否则显示全屏大关机界面
            if (m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode) {
                m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
            }
        }
        return true;
    }
    return false;
}

void LockFrame::showUserList()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
    QTimer::singleShot(10, this, [ = ] {
        this->show();
    });
}

void LockFrame::showLockScreen()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
    show();
}

void LockFrame::showShutdown()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    show();
}

void LockFrame::shutdownInhibit(const SessionBaseModel::PowerAction action, bool needConfirm)
{
    //如果其他显示屏界面已经检查过是否允许关机，此显示屏上的界面不再显示，避免重复检查并触发信号
    if (m_model->isCheckedInhibit()) return;
    //记录多屏状态下当前显示屏是否显示内容
    bool old_visible = contentVisible();

    if (!m_warningContent)
        m_warningContent = new WarningContent(m_model, action, this);
    else
        m_warningContent->setPowerAction(action);
    m_warningContent->setFixedSize(size());
    setContent(m_warningContent);

    //多屏状态下，当前界面显示内容时才显示提示界面
    if (old_visible) {
        setContentVisible(true);
        m_lockContent->hide();
    }

    //检查是否允许关机
    m_warningContent->beforeInvokeAction(needConfirm);
}

void LockFrame::cancelShutdownInhibit()
{
    //允许关机检查结束后切换界面
    //记录多屏状态下当前显示屏是否显示内容
    bool old_visible = contentVisible();

    setContent(m_lockContent);

    //隐藏提示界面
    if (m_warningContent) {
        m_warningContent->hide();
    }

    //多屏状态下，当前界面显示内容时才显示
    if (old_visible) {
        setContentVisible(true);
    }
}

void LockFrame::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:    qApp->quit();       break;
#endif
    }
}

void LockFrame::showEvent(QShowEvent *event)
{
    emit requestEnableHotzone(false);

    m_model->setVisible(true);

    return FullscreenBackground::showEvent(event);
}

void LockFrame::hideEvent(QHideEvent *event)
{
    emit requestEnableHotzone(true);

    m_model->setVisible(false);

    return FullscreenBackground::hideEvent(event);
}

LockFrame::~LockFrame()
{

}
