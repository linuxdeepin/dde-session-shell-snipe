// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waypointwidget.h"
#include "waypointmodel.h"

#include <QPainter>
#include <QEvent>
#include <QMouseEvent>

using namespace gestureLogin;
WayPointWidget::WayPointWidget(QWidget *parent)
    : QWidget(parent)
    , m_isSelected(false)
{
    setContentsMargins(0, 0, 0, 0);

    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(WayPointModel::instance(), &WayPointModel::widgetColorChanged, this, [this] {
        update();
    });
}

void WayPointWidget::setId(int id)
{
    m_id = id;
}

void WayPointWidget::onSelectedById(int id, bool selected)
{
    if (id == m_id || id == -1) {
        m_isSelected = selected;
        update();
    }
}

void WayPointWidget::onPathStarted(const QPoint &startPoint)
{
    if (geometry().contains(startPoint)) {
        Q_EMIT arrived(this->m_id);
    }

    m_started = true;
}

void WayPointWidget::onPathStopped()
{
    m_started = false;
}

void WayPointWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainterPath pathBound;
    QPainterPath pathCenter;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    static auto model = WayPointModel::instance();
    auto colorConfig = model->colorConfig();

    QColor unselectedBoarderColor = colorConfig.unSelecteBoarder;
    QColor selectedBoarderColor = colorConfig.selectedBoarder;
    QColor fillColor = colorConfig.fill;
    QColor internalFillColor = colorConfig.internalFill;

    if (model->showErrorStyle()) {
        selectedBoarderColor = colorConfig.warningLine;
        fillColor = colorConfig.warningFill;
        internalFillColor = selectedBoarderColor;
    }

    pathBound.addEllipse(this->contentsRect().center(), 23, 23);
    QPen pen;
    pen.setColor(m_isSelected ? selectedBoarderColor : unselectedBoarderColor);
    pen.setWidth(2);
    painter.setPen(pen);

    if (m_isSelected) {
        painter.setBrush(fillColor);
    }
    painter.drawPath(pathBound);

    if (m_isSelected) {
        pathCenter.addEllipse(this->contentsRect().center(), 12, 12);
        painter.setBrush(internalFillColor);
        painter.drawPath(pathCenter);
    }
}
