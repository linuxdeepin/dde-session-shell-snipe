// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "roundpopupwidget.h"

#include <QPainter>
#include <QDebug>
#include <QVBoxLayout>

static constexpr int MARGIN = 5;
static constexpr int RADIUS = 18;

RoundPopupWidget::RoundPopupWidget(QWidget *parent)
    : DBlurEffectWidget(parent)
    , m_pContentWidget(nullptr)
    , m_mainLayout(new QVBoxLayout())
    , m_savedParent(this)
{
    initUI();
}

void RoundPopupWidget::initUI()
{
    setBlurRectXRadius(RADIUS);
    setBlurRectYRadius(RADIUS);
    setMaskColor(QColor(238, 238, 238, 0.8 * 255));
    m_mainLayout->setMargin(0);
    m_mainLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    setLayout(m_mainLayout);
}

void RoundPopupWidget::setContent(QWidget *widget)
{
    if (widget != m_pContentWidget && m_pContentWidget) {
        m_mainLayout->removeWidget(m_pContentWidget);
        m_pContentWidget->setParent(m_savedParent);
        m_pContentWidget->hide();
        m_savedParent = this;
    }

    if (!widget)
        return;

    setFixedSize(widget->size() + QSize(MARGIN * 2, MARGIN * 2));
    if (widget->parentWidget() != this)
        m_savedParent = widget->parentWidget();
    m_mainLayout->addWidget(widget);
    m_pContentWidget = widget;
    widget->show();
}

QWidget *RoundPopupWidget::getContent() const
{
    return m_pContentWidget;
}

void RoundPopupWidget::hideEvent(QHideEvent *event)
{
    setContent(nullptr);
    QWidget::hideEvent(event);
}
