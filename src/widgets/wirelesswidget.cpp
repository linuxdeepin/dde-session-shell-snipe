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
    : DFrame(parent)
    , m_localeName(locale)
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

    initWireless();
}

void WirelessWidget::initConnect(DtkWirelessPage *wirelessPage)
{
    connect(wirelessPage, &WirelessPage::requestConnectAp, m_networkWorker, &NetworkWorker::activateAccessPoint);
    connect(wirelessPage, &WirelessPage::requestDeviceEnabled, m_networkWorker, &NetworkWorker::setDeviceEnable);
    connect(wirelessPage, &WirelessPage::requestWirelessScan, m_networkWorker, &NetworkWorker::requestWirelessScan);
}

void WirelessWidget::onDeviceChanged(DtkWirelessDev *dev, bool isNewDev)
{
    QString devPath = dev->uni();
    if (isNewDev && !m_mapWirelessPage.contains(devPath)) {
        createNewWirelessPage(dev);
    } else if (!isNewDev && m_mapWirelessPage.contains(devPath)) {
        DtkWirelessPage *page = m_mapWirelessPage.take(devPath);
        page->deleteLater();
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

void WirelessWidget::initWireless()
{
    m_mapWirelessPage.clear();
    for (auto dev : m_networkWorker->devices()) {
        createNewWirelessPage(dev);
    }
}

void WirelessWidget::createNewWirelessPage(DtkWirelessDev *device)
{
    DtkWirelessPage *page = new DtkWirelessPage(m_localeName, device, this);

    page->setWorker(m_networkWorker);
    initConnect(page);

    m_mapWirelessPage[device->uni()] = page;
    m_boxWidget->addWidget(page);
}

void WirelessWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

void WirelessWidget::updateLocale(std::shared_ptr<User> user)
{
    m_localeName = user->locale();
}
