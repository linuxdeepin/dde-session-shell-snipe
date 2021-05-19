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

WirelessWidget::WirelessWidget(QWidget *parent)
    : DFrame(parent)
{
    this->setFixedSize(420, 410);
    setBackgroundRole(QPalette::Background);
    setFrameRounded(true);

    QPalette p = palette();
    p.setColor(QPalette::Background, QColor(255, 255, 255, 12));
    setPalette(p);

    m_mainLayout = new QVBoxLayout();
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_mainLayout);

    QScrollArea *area = new QScrollArea(this);
    area->viewport()->setAutoFillBackground(false);
    area->setFrameStyle(QFrame::NoFrame);
    area->setWidgetResizable(true);
    area->setFocusPolicy(Qt::NoFocus);

    m_boxWidget = new DVBoxWidget(area);
    QPalette p2 = m_boxWidget->palette();
    p2.setColor(QPalette::Background, Qt::transparent);
    m_boxWidget->setPalette(p2);

    area->setWidget(m_boxWidget);
    m_mainLayout->addWidget(area);

    init();

    connect(m_networkWorker, &NetworkWorker::deviceChaged, this, &WirelessWidget::onDeviceChanged);
}

void WirelessWidget::init()
{
    m_networkWorker = new NetworkWorker();
    m_networkWorker->moveToThread(qApp->thread());

    onDeviceChanged();
}

void WirelessWidget::initConnect(QPointer<dtk::wireless::WirelessPage> wirelessPage)
{
    connect(wirelessPage, &WirelessPage::requestConnectAp, m_networkWorker, &NetworkWorker::activateAccessPoint);
    connect(wirelessPage, &WirelessPage::requestDeviceEnabled, m_networkWorker, &NetworkWorker::setDeviceEnable);
    connect(wirelessPage, &WirelessPage::requestWirelessScan, m_networkWorker, &NetworkWorker::requestWirelessScan);
    connect(wirelessPage, &WirelessPage::requestRefreshWiFiStrengthDisplay, this, &WirelessWidget::signalStrengthChanged);
}

void WirelessWidget::onDeviceChanged()
{
    if (m_wirelessPage) {
        m_wirelessPage->hide();
        delete m_wirelessPage;
    } else {
        for (auto dev : m_networkWorker->devices()) {
            WirelessPage *page = new WirelessPage(dev, this);
            page->setWorker(m_networkWorker);
            m_boxWidget->addWidget(page);

            page->updateWiFiStrengthDisplay();
            initConnect(page);
        }
    }
}

WirelessWidget *WirelessWidget::getInstance()
{
    static WirelessWidget w;
    return &w;
}

WirelessWidget::~WirelessWidget()
{
}

void WirelessWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

