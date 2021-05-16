/*
  * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
  *
  * Author:     dengbo <dengbo@uniontech.com>
  *
  * Maintainer: dengbo <dengbo@uniontech.com>
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

#include "wirelesswidget.h"
#include <QLEInteger>

using namespace dtk::wireless;

WirelessWidget::WirelessWidget(const QString locale, QWidget *parent)
    : QWidget(parent)
    , m_localeName(locale)
{
    this->setFixedSize(420, 410);
    setWindowFlags(Qt::Widget | windowFlags());

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(m_mainLayout);

    init();
    initConnect();
}

void WirelessWidget::init()
{
    m_networkWorker = new NetworkWorker();
    m_networkWorker->moveToThread(qApp->thread());

    onDeviceChanged();
}

void WirelessWidget::initConnect()
{
    connect(m_wirelessPage, &WirelessPage::requestConnectAp, m_networkWorker, &NetworkWorker::activateAccessPoint);
    connect(m_wirelessPage, &WirelessPage::requestDeviceEnabled, m_networkWorker, &NetworkWorker::setDeviceEnable);
    connect(m_wirelessPage, &WirelessPage::requestWirelessScan, m_networkWorker, &NetworkWorker::requestWirelessScan);
    connect(m_networkWorker, &NetworkWorker::deviceChaged, this, &WirelessWidget::onDeviceChanged);
    connect(m_wirelessPage, &WirelessPage::requestRefreshWiFiStrengthDisplay, this, &WirelessWidget::signalStrengthChanged);
}

void WirelessWidget::onDeviceChanged()
{
    if (m_wirelessPage) {
        m_wirelessPage->hide();
        delete m_wirelessPage;
    } else {
        for (auto dev : m_networkWorker->devices()) {
            m_wirelessPage = new WirelessPage(m_localeName, dev, this);
            m_wirelessPage->setWorker(m_networkWorker);
            m_mainLayout->addWidget(m_wirelessPage);

            m_wirelessPage->updateWiFiStrengthDisplay();
        }
    }
}

WirelessWidget *WirelessWidget::getInstance(const QString locale)
{
    static WirelessWidget w(locale);
    return &w;
}

WirelessWidget::~WirelessWidget()
{
}

void WirelessWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

void WirelessWidget::updateLocale(std::shared_ptr<User> user)
{
    m_localeName = user->locale();
}

