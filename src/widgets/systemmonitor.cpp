// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemmonitor.h"

#include <QHBoxLayout>

#include <DHiDPIHelper>
#include <DFontSizeManager>

DWIDGET_USE_NAMESPACE

SystemMonitor::SystemMonitor(QWidget *parent)
    : ActionWidget(parent)
    , m_icon(new QLabel(this))
    , m_text(new QLabel(this))
{
    setObjectName(QStringLiteral("SystemMonitor"));
    setAccessibleName(QStringLiteral("SystemMonitor"));

    setMinimumHeight(40);

    initUI();
}

void SystemMonitor::initUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(35, 0, 35, 0);

    m_icon->setPixmap(DHiDPIHelper::loadNxPixmap(":/img/deepin-system-monitor.svg"));
    m_text->setText(tr("Start system monitor"));

    mainLayout->addWidget(m_icon, 0, Qt::AlignVCenter | Qt::AlignRight);
    mainLayout->addWidget(m_text, 1, Qt::AlignVCenter | Qt::AlignLeft);

    DFontSizeManager::instance()->bind(m_text, DFontSizeManager::T6);
}