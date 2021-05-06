/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     justforlxz <justforlxz@outlook.com>
 *
 * Maintainer: justforlxz <justforlxz@outlook.com>
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

#include "logincontent.h"
#include "src/widgets/sessionwidget.h"
#include "src/widgets/controlwidget.h"

LoginContent::LoginContent(SessionBaseModel *const model, QWidget *parent)
    : LockContent(model, parent)
    , m_wirelessWigdet(nullptr)
{
    m_sessionFrame = new SessionWidget;
    m_wirelessConcealWidget = new QWidget(this);
    m_sessionFrame->setModel(model);
    m_controlWidget->setSessionSwitchEnable(m_sessionFrame->sessionCount() > 1);
    m_controlWidget->setWirelessListEnable(true);

    connect(m_sessionFrame, &SessionWidget::hideFrame, this, &LockContent::restoreMode);
    connect(m_sessionFrame, &SessionWidget::sessionChanged, this, &LockContent::restoreMode);
    connect(m_controlWidget, &ControlWidget::updateWirelessDisplay, this, &LoginContent::updateWirelessDisplay);
    connect(m_controlWidget, &ControlWidget::requestSwitchSession, this, [ = ] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::SessionMode);
    });
    connect(m_controlWidget, &ControlWidget::requestWiFiPage, this, [ = ] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::WirelessMode);
        onRequestWirelessPage();
    });
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, m_controlWidget, &ControlWidget::chooseToSession);
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, this, &LockContent::restoreMode);
    connect(m_model, &SessionBaseModel::hideWirelessWidget, this, [this]() {
        if (m_wirelessWigdet) {
            m_wirelessWigdet->setVisible(false);
        }
    });
}

void LoginContent::onRequestWirelessPage()
{
    if (!m_wirelessWigdet) {
        QTimer::singleShot(0, this, [ = ] {
            m_wirelessWigdet = WirelessWidget::getInstance();
            qDebug() << "init WirelessWidget over." << m_wirelessWigdet;

            connect(m_wirelessWigdet, &WirelessWidget::signalStrengthChanged, m_controlWidget, &ControlWidget::updateWifiIconDisplay);
            onRequestWirelessPage();
        });
        return;
    }

    m_wirelessWigdet->setParent(this);
    m_wirelessWigdet->raise();

    updateWirelessListPosition();
    m_wirelessWigdet->setVisible(true);
}

void LoginContent::updateWirelessListPosition()
{
    const QPoint point = mapToParent(QPoint((width() - m_wirelessWigdet->width()) / 2, (height() - m_wirelessWigdet->height()) / 2));
    m_wirelessWigdet->move(point);
}

void LoginContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (user.get() == nullptr) return;

    LockContent::onCurrentUserChanged(user);
    m_sessionFrame->switchToUser(user->name());
}

void LoginContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    switch (status) {
    case SessionBaseModel::ModeStatus::SessionMode:
        pushSessionFrame();
        break;
    case SessionBaseModel::ModeStatus::WirelessMode:
        pushWirelessFrame();
        break;
    default:
        LockContent::onStatusChanged(status);
        break;
    }
}

void LoginContent::pushSessionFrame()
{
    QSize size = getCenterContentSize();
    m_sessionFrame->setFixedSize(size);
    setCenterContent(m_sessionFrame);
}

void LoginContent::pushWirelessFrame()
{

    setCenterContent(m_wirelessConcealWidget);
}

void LoginContent::updateWirelessDisplay()
{
    if (!m_wirelessWigdet) {
        m_wirelessWigdet = WirelessWidget::getInstance();
    }

    if (m_wirelessWigdet) {
        bool isDeviceExist = m_controlWidget->getWirelessDeviceExistFlg();

        if (!isDeviceExist) {
            m_wirelessWigdet->setVisible(false);
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        }
    }
}
