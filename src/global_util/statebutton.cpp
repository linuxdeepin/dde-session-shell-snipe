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

#include "statebutton.h"

#include <QIcon>
#include <QPainter>
#include <QtMath>

StateButton::StateButton(QWidget *parent)
    : QWidget(parent)
    , m_type(Check)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void StateButton::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int radius = qMin(width(), height());
    painter.setPen(QPen(Qt::NoPen));
    painter.setBrush(palette().color(QPalette::Highlight));
    painter.drawPie(rect(), 0, 360 * 16);

    QPen pen(Qt::white, radius / 100.0 * 6.20, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    drawCheck(painter, pen, radius);
}

void StateButton::drawCheck(QPainter &painter, QPen &pen, int radius)
{
    painter.setPen(pen);

    QPointF points[3] = {
        QPointF(radius / 100.0 * 32,  radius / 100.0 * 57),
        QPointF(radius / 100.0 * 45,  radius / 100.0 * 70),
        QPointF(radius / 100.0 * 75,  radius / 100.0 * 35)
    };

    painter.drawPolyline(points, 3);
}

