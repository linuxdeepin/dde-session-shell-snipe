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

#include "controlwidget.h"

#include <QHBoxLayout>
#include <dimagebutton.h>
#include <QEvent>
#include <QWheelEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainter>

#include <networkmanagerqt/manager.h>

#define BUTTON_ICON_SIZE QSize(26,26)
#define BUTTON_SIZE QSize(52,52)

DWIDGET_USE_NAMESPACE

ControlWidget::ControlWidget(QWidget *parent) : QWidget(parent)
  , m_systemNetworkInter("com.deepin.system.Network", "/com/deepin/system/Network", QDBusConnection::systemBus(), this)
{
    initUI();
    initConnect();
}

void ControlWidget::setVirtualKBVisible(bool visible)
{
    m_virtualKBBtn->setVisible(visible);
}

void ControlWidget::initUI()
{
    setFocusPolicy(Qt::TabFocus);

    m_mainLayout = new QHBoxLayout;

    m_virtualKBBtn = new DFloatingButton(this);
    m_virtualKBBtn->setIcon(QIcon::fromTheme(":/img/screen_keyboard_normal.svg"));
    m_virtualKBBtn->hide();
    m_virtualKBBtn->setIconSize(BUTTON_ICON_SIZE);
    m_virtualKBBtn->setFixedSize(BUTTON_SIZE);
    m_virtualKBBtn->setFocusPolicy(Qt::ClickFocus);
    m_virtualKBBtn->setAutoExclusive(true);
    m_virtualKBBtn->setBackgroundRole(DPalette::Button);
    m_virtualKBBtn->installEventFilter(this);

    m_switchUserBtn = new DFloatingButton(this);
    m_switchUserBtn->setIcon(QIcon::fromTheme(":/img/bottom_actions/userswitch_normal.svg"));
    m_switchUserBtn->setIconSize(BUTTON_ICON_SIZE);
    m_switchUserBtn->setFixedSize(BUTTON_SIZE);
    m_switchUserBtn->setFocusPolicy(Qt::ClickFocus);
    m_switchUserBtn->setAutoExclusive(true);
    m_switchUserBtn->setBackgroundRole(DPalette::Button);
    m_switchUserBtn->installEventFilter(this);

    m_powerBtn = new DFloatingButton(this);
    m_powerBtn->setIcon(QIcon(":/img/bottom_actions/shutdown_normal.svg"));
    m_powerBtn->setIconSize(BUTTON_ICON_SIZE);
    m_powerBtn->setFixedSize(BUTTON_SIZE);
    m_powerBtn->setFocusPolicy(Qt::ClickFocus);
    m_powerBtn->setAutoExclusive(true);
    m_powerBtn->setBackgroundRole(DPalette::Button);
    m_powerBtn->installEventFilter(this);

    m_btnList.append(m_virtualKBBtn);
    m_btnList.append(m_switchUserBtn);
    m_btnList.append(m_powerBtn);

    m_mainLayout->setMargin(0);
    m_mainLayout->setSpacing(26);
    m_mainLayout->addStretch();
    m_mainLayout->addWidget(m_virtualKBBtn, 0, Qt::AlignBottom);
    m_mainLayout->addWidget(m_switchUserBtn, 0, Qt::AlignBottom);
    m_mainLayout->addWidget(m_powerBtn, 0, Qt::AlignBottom);
    m_mainLayout->addSpacing(60);

    setLayout(m_mainLayout);
}

void ControlWidget::initConnect()
{
    connect(m_switchUserBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchUser);
    connect(m_powerBtn, &DFloatingButton::clicked, this, &ControlWidget::requestShutdown);
    connect(m_virtualKBBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchVirtualKB);
    // refresh button
    connect(&m_systemNetworkInter, &SystemNetworkInter::DeviceEnabled, this, [=](const QDBusObjectPath & path, bool enabled){
        // check if type is wifi
        if (NetworkManager::Device(path.path()).type() != NetworkManager::Device::Wifi)
            return;
        // check if already exist
        if (enabled && m_enabledDevices.indexOf(path.path(), 0) == -1)
            m_enabledDevices.append(path.path());
        else if (!enabled)
            m_enabledDevices.removeAll(path.path());

        // check size
        if (m_enabledDevices.size() > 0)
            this->setWirelessListEnable(true);
        else
            this->setWirelessListEnable(false);
    });
    updateWirelessListEnabled();
}

void ControlWidget::showTips()
{
#ifndef SHENWEI_PLATFORM
    m_tipsAni->setStartValue(QPoint(m_tipWidget->width(), 0));
    m_tipsAni->setEndValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(0, 0);
#endif
}

void ControlWidget::hideTips()
{
#ifndef SHENWEI_PLATFORM
    //在退出动画时候会出现白边，+1
    m_tipsAni->setEndValue(QPoint(m_tipWidget->width() + 1, 0));
    m_tipsAni->setStartValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(m_tipWidget->width() + 1, 0);
#endif
}

void ControlWidget::setUserSwitchEnable(const bool visible)
{
    m_switchUserBtn->setVisible(visible);
    if (!visible) {
        m_focusState = FocusNo;
        setFocusPolicy(Qt::TabFocus);
    }
    if (m_btnList.indexOf(m_switchUserBtn) == m_index) {
        m_index = 0;
    }
}

void ControlWidget::setWirelessListEnable(const bool visible)
{
    if (!m_wirelessBtn) {
        m_wirelessBtn = new DFloatingButton(this);
        m_wirelessBtn->setIcon(QIcon::fromTheme(":/img/wireless/wireless-80-symbolic.svg"));
        m_wirelessBtn->setIconSize(BUTTON_ICON_SIZE);
        m_wirelessBtn->setFixedSize(BUTTON_SIZE);
        m_wirelessBtn->setFocusPolicy(Qt::ClickFocus);
        m_wirelessBtn->setAutoExclusive(true);
        m_wirelessBtn->setBackgroundRole(DPalette::Button);
        m_wirelessBtn->installEventFilter(this);

        updateWirelessBtnDisplay(QString(""));

        m_mainLayout->insertWidget(2, m_wirelessBtn);
        m_mainLayout->setAlignment(m_wirelessBtn, Qt::AlignBottom);

        m_btnList.push_front(m_wirelessBtn);

        connect(m_wirelessBtn, &DFloatingButton::clicked, this, &ControlWidget::requestWiFiPage);
        connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceAdded, this, &ControlWidget::updateWirelessBtnDisplay);
        connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceRemoved, this, &ControlWidget::updateWirelessBtnDisplay);
    }

    if (visible)
        m_wirelessBtn->setIcon(QIcon::fromTheme(":/img/wireless/wireless-80-symbolic.svg"));
    else
        m_wirelessBtn->setIcon(QIcon::fromTheme(":/img/wireless/Login-network-wirelss-no-route-symbolic.svg"));
}

/**
 * @brief 当有WiFi设备添加或者移除时,更新登录界面WiFi按钮显示状态
 *
 * @param QString path为了匹配相应的槽函数,暂未使用到
 * @return void
 */
void ControlWidget::updateWirelessBtnDisplay(const QString &path)
{
    Q_UNUSED(path);
    for (auto device : NetworkManager::networkInterfaces()) {
        if (device->type() == NetworkManager::Device::Type::Wifi) {
            m_wirelessBtn->setVisible(true);
            m_wifiDeviceExist = true;

            emit updateWirelessDisplay();
            return;
        }
    }

    m_wirelessBtn->setVisible(false);
    m_wifiDeviceExist = false;
    emit updateWirelessDisplay();
}

void ControlWidget::setSessionSwitchEnable(const bool visible)
{
    if (!visible) return;

    if (!m_sessionBtn) {
        m_sessionBtn = new DFloatingButton(this);
        m_sessionBtn->setIconSize(BUTTON_ICON_SIZE);
        m_sessionBtn->setFixedSize(BUTTON_SIZE);
        m_sessionBtn->setAutoExclusive(true);
        m_sessionBtn->setBackgroundRole(DPalette::Button);
        m_sessionBtn->setFocusPolicy(Qt::ClickFocus);
#ifndef SHENWEI_PLATFORM
        m_sessionBtn->installEventFilter(this);
#else
        m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
        m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
        m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");

#endif

        m_mainLayout->insertWidget(1, m_sessionBtn);
        m_mainLayout->setAlignment(m_sessionBtn, Qt::AlignBottom);
        m_btnList.push_front(m_sessionBtn);

        connect(m_sessionBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchSession);
    }

    if (!m_tipWidget) {
        m_tipWidget = new QWidget;
        m_mainLayout->insertWidget(1, m_tipWidget);
        m_mainLayout->setAlignment(m_tipWidget, Qt::AlignCenter);
    }

    if (!m_sessionTip) {
        m_sessionTip = new QLabel(m_tipWidget);
        m_sessionTip->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_sessionTip->installEventFilter(this);

#ifndef SHENWEI_PLATFORM
        m_sessionTip->setStyleSheet("color:white;"
                                    "font-size:12px;");
#else
        QPalette pe;
        pe.setColor(QPalette::WindowText, Qt::white);
        m_sessionTip->setPalette(pe);
#endif

        QGraphicsDropShadowEffect *tipsShadow = new QGraphicsDropShadowEffect(m_sessionTip);
        tipsShadow->setColor(Qt::white);
        tipsShadow->setBlurRadius(4);
        tipsShadow->setOffset(0, 0);
        m_sessionTip->setGraphicsEffect(tipsShadow);

        m_sessionTip->move(m_tipWidget->width(), 0);
    }

#ifndef SHENWEI_PLATFORM
    if (!m_tipsAni) {
        m_tipsAni = new QPropertyAnimation(m_sessionTip, "pos", this);
    }
#endif
}

void ControlWidget::updateWirelessListEnabled()
{
    for (auto dev : NetworkManager::networkInterfaces()) {
        // check if type is wifi
        if (dev->type() != NetworkManager::Device::Wifi)
            continue;
        // check if device is enabled
        if (m_systemNetworkInter.IsDeviceEnabled(dev->uni())
                && m_enabledDevices.indexOf(dev->uni(), 0) == -1)
            m_enabledDevices.append(dev->uni());
    }

    // check size
    if (m_enabledDevices.size() > 0)
        this->setWirelessListEnable(true);
    else
        this->setWirelessListEnable(false);
}

void ControlWidget::chooseToSession(const QString &session)
{
    if (m_sessionBtn && m_sessionTip) {
        qDebug() << "chosen session: " << session;
        if (session.isEmpty())
            return;

        m_sessionTip->setText(session);
        m_sessionTip->adjustSize();
        //当session长度改变时，应该移到它的width来隐藏
        m_sessionTip->move(m_sessionTip->size().width() + 1, 0);
        const QString sessionId = session.toLower();
        const QString normalIcon = QString(":/img/sessions/%1_indicator_normal.svg").arg(sessionId);

        if (QFile(normalIcon).exists()) {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(normalIcon));
#else
            m_sessionBtn->setProperty("normalIcon", normalIcon);
            m_sessionBtn->setProperty("hoverIcon", hoverIcon);
            m_sessionBtn->setProperty("checkedIcon", checkedIcon);
#endif
        } else {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(":/img/sessions/unknown_indicator_normal.svg"));
#else
            m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
            m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
            m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");
#endif
        }
    }
}

void ControlWidget::leftKeySwitch()
{
    if (m_index == 0) {
        m_index = m_btnList.length() - 1;
    } else {
        --m_index;
    }

    for (int i = m_btnList.size(); i != 0; --i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

void ControlWidget::rightKeySwitch()
{
    if (m_index == m_btnList.size() - 1) {
        m_index = 0;
    } else {
        ++m_index;
    }

    for (int i = 0; i < m_btnList.size(); ++i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

bool ControlWidget::eventFilter(QObject *watched, QEvent *event)
{
#ifndef SHENWEI_PLATFORM
    if (watched == m_sessionBtn) {
        if (event->type() == QEvent::Enter)
            showTips();
        else if (event->type() == QEvent::Leave)
            hideTips();
    }

    if (watched == m_sessionTip) {
        if (event->type() == QEvent::Resize) {
            m_tipWidget->setFixedSize(m_sessionTip->size());
        }
    }

    DFloatingButton *btn = dynamic_cast<DFloatingButton *>(watched);
    if (m_btnList.contains(btn)) {
        if (event->type() == QEvent::Enter) {
            m_index = m_btnList.indexOf(btn);
        }
        if (btn->isVisible()) {
            if (event->type() == QFocusEvent::FocusOut) {
                m_focusState = FocusNo;
                setFocusPolicy(Qt::TabFocus);
            } else if (event->type() == QFocusEvent::FocusIn) {
                m_focusState = FocusHasIn;
                setFocusPolicy(Qt::NoFocus);
            }
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return false;
}

void ControlWidget::focusInEvent(QFocusEvent *)
{
    switch (m_focusState) {
    case FocusNo: {
        m_focusState = FocusHasIn;
        clearFocus();
    }
    break;
    case FocusHasIn: {
        m_focusState = FocusReadyOut;
        clearFocus();
    }
    break;
    case FocusReadyOut: {

    }
    break;
    }
}

void ControlWidget::focusOutEvent(QFocusEvent *)
{
    switch (m_focusState) {
    case FocusNo: {

    }
    break;
    case FocusHasIn: {
        m_btnList.at(m_index)->setFocus();
    }
    break;
    case FocusReadyOut: {
        m_focusState = FocusNo;
    }
    break;
    }
}

void ControlWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        leftKeySwitch();
        break;
    case Qt::Key_Right:
        rightKeySwitch();
        break;
    default:
        break;
    }
}
