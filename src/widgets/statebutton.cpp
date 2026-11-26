// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "statebutton.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

StateButton::StateButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
    , m_normalTextColor(Qt::white)
    , m_hoverTextColor(Qt::black)
    , m_pressTextColor(QColor(0, 129, 255))
    , m_normalBackground(QColor(255, 255, 255, 25))
    , m_hoverBackground(QColor(255, 255, 255, 102))
    , m_pressBackground(QColor(255, 255, 255, 51))
    , m_borderRadius(8)
    , m_hovered(false)
    , m_pressed(false)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

StateButton::~StateButton()
{
}

void StateButton::setNormalTextColor(const QColor &color)
{
    m_normalTextColor = color;
    updateTextColor();
}

void StateButton::setHoverTextColor(const QColor &color)
{
    m_hoverTextColor = color;
    updateTextColor();
}

void StateButton::setPressTextColor(const QColor &color)
{
    m_pressTextColor = color;
    updateTextColor();
}

void StateButton::setNormalBackground(const QColor &color)
{
    m_normalBackground = color;
    updateBackground();
}

void StateButton::setHoverBackground(const QColor &color)
{
    m_hoverBackground = color;
    updateBackground();
}

void StateButton::setPressBackground(const QColor &color)
{
    m_pressBackground = color;
    updateBackground();
}

void StateButton::setBorderRadius(int radius)
{
    m_borderRadius = radius;
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void StateButton::enterEvent(QEnterEvent *event)
#else
void StateButton::enterEvent(QEvent *event)
#endif
{
    m_hovered = true;
    updateTextColor();
    updateBackground();
    QPushButton::enterEvent(event);
}

void StateButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    m_pressed = false;
    updateTextColor();
    updateBackground();
    QPushButton::leaveEvent(event);
}

void StateButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        updateTextColor();
        updateBackground();
    }
    QPushButton::mousePressEvent(event);
}

void StateButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        updateTextColor();
        updateBackground();
    }
    QPushButton::mouseReleaseEvent(event);
}

void StateButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor;
    if (m_pressed) {
        bgColor = m_pressBackground;
    } else if (m_hovered) {
        bgColor = m_hoverBackground;
    } else {
        bgColor = m_normalBackground;
    }

    QPainterPath path;
    path.addRoundedRect(rect(), m_borderRadius, m_borderRadius);
    painter.fillPath(path, bgColor);

    QColor textColor;
    if (m_pressed) {
        textColor = m_pressTextColor;
    } else if (m_hovered) {
        textColor = m_hoverTextColor;
    } else {
        textColor = m_normalTextColor;
    }

    painter.setPen(textColor);
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignCenter, text());
}

void StateButton::updateTextColor()
{
    QPalette pal = palette();
    if (m_pressed) {
        pal.setColor(QPalette::ButtonText, m_pressTextColor);
    } else if (m_hovered) {
        pal.setColor(QPalette::ButtonText, m_hoverTextColor);
    } else {
        pal.setColor(QPalette::ButtonText, m_normalTextColor);
    }
    setPalette(pal);
}

void StateButton::updateBackground()
{
    update();
}
