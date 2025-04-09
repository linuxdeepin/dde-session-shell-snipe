// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "actionwidget.h"

#include <DHiDPIHelper>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>

ActionWidget::ActionWidget(QWidget *parent)
    : QWidget(parent)
    , m_state(Leave)
    , m_radius(8)
{
}

void ActionWidget::setState(const State state)
{
    m_state = state;
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ActionWidget::enterEvent(QEnterEvent *event)
#else
void ActionWidget::enterEvent(QEvent *event)
#endif
{
    Q_EMIT mouseEnter();
    m_state = Enter;
    update();

    QWidget::enterEvent(event);
}

void ActionWidget::leaveEvent(QEvent *event)
{
    Q_EMIT mouseLeave();
    m_state = Leave;
    update();

    QWidget::leaveEvent(event);
}

void ActionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_state = Release;
    Q_EMIT requestAction();
    update();

    QWidget::mouseReleaseEvent(event);
}

void ActionWidget::mousePressEvent(QMouseEvent *event)
{
    m_state = Press;
    update();

    QWidget::mousePressEvent(event);
}

void ActionWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        m_state = Press;
    }
    update();

    QWidget::keyReleaseEvent(event);
}

void ActionWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        emit requestAction();
        m_state = Release;
    }
    update();

    QWidget::keyReleaseEvent(event);
}

void ActionWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing, true);

    switch (m_state) {
    case Enter:
        painter.setBrush(QColor(255, 255, 255, 0.3 * 255));
        break;
    case Leave:
        painter.setBrush(QColor(255, 255, 255, 0.1 * 255));
        break;
    case Release:
        painter.setBrush(QColor(255, 255, 255, 0.3 * 255));
        break;
    case Press:
        painter.setBrush(QColor(255, 255, 255, 0.1 * 255));
        break;
    }
    painter.drawRoundedRect(rect(), m_radius, m_radius);

    QWidget::paintEvent(event);
}
