// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inhibitbutton.h"

#include <QPainter>
#include <QDebug>
#include <QLabel>
#include <QPixmap>
#include <QLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QStyleOption>

const int penWidth = 2;

InhibitButton::InhibitButton(QWidget *parent)
    : QWidget(parent)
    , m_isSelected(false)
    , m_state(Leave)
    , m_iconLabel(new QLabel(this))
    , m_textLabel(new QLabel(this))
{
    m_iconLabel->setFixedSize(64, 64);
    m_textLabel->setFixedHeight(64);

    m_iconLabel->setAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignVCenter);
    m_textLabel->setAlignment(Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignVCenter);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_iconLabel);
    layout->setSpacing(20);
    layout->addWidget(m_textLabel);

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    this->setPalette(palette);

    this->setLayout(layout);
}

InhibitButton::~InhibitButton()
{

}

void InhibitButton::setText(const QString &text)
{
    m_textLabel->setText(text);
}

QString InhibitButton::text()
{
    return m_textLabel->text();
}

void InhibitButton::setNormalPixmap(const QPixmap &normalPixmap)
{
    m_normalPixmap = normalPixmap;
}

void InhibitButton::setHoverPixmap(const QPixmap &hoverPixmap)
{
    m_hoverPixmap = hoverPixmap;
}

QPixmap InhibitButton::hoverPixmap()
{
    return m_hoverPixmap;
}

QPixmap InhibitButton::normalPixmap()
{
    return m_normalPixmap;
}

void InhibitButton::setState(State state)
{
    m_state = state;
}

void InhibitButton::setSelected(bool isSelected)
{
    m_isSelected = isSelected;
    setState(isSelected ? Enter : Leave);
    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, m_isSelected ? Qt::black : Qt::white);
    this->setPalette(palette);

    m_iconLabel->setPixmap(m_isSelected ? m_hoverPixmap : m_normalPixmap);
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void InhibitButton::enterEvent(QEnterEvent *event)
#else
void InhibitButton::enterEvent(QEvent *event)
#endif
{
    m_state = Enter;
    m_isSelected = true;
    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    this->setPalette(palette);

    m_iconLabel->setPixmap(m_hoverPixmap);
    Q_EMIT mouseEnter();

    QWidget::enterEvent(event);
    update();
}

void InhibitButton::leaveEvent(QEvent *event)
{
    m_state = Leave;
    m_isSelected = false;
    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    this->setPalette(palette);

    m_iconLabel->setPixmap(m_normalPixmap);
    Q_EMIT mouseLeave();

    QWidget::leaveEvent(event);
    update();
}

void InhibitButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPalette palette = this->palette();
    QColor color = palette.color(QPalette::Text);
    switch (m_state) {
    case Enter:
    case Release:
        color.setAlphaF(0.3);
        break;
    case Leave:
    case Press:
        color.setAlphaF(0.1);
        break;
    }
    painter.setBrush(color);

    QStyleOption opt;
    opt.initFrom(this);
    if (opt.state & QStyle::State_HasFocus) {
        QPen pen;
        pen.setWidth(penWidth);
        color.setAlphaF(0.5);
        pen.setColor(color);
        painter.setPen(pen);
    } else
        painter.setPen(Qt::NoPen);

    painter.drawRoundedRect(rect().marginsRemoved(QMargins(penWidth, penWidth, penWidth, penWidth)), 18, 18);

    QWidget::paintEvent(event);
}

void InhibitButton::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit clicked();
    return;
}

void InhibitButton::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit clicked();
        break;
    }
    QWidget::keyPressEvent(event);
}

void InhibitButton::showEvent(QShowEvent *event)
{
    m_iconLabel->setPixmap(m_normalPixmap);
    QWidget::showEvent(event);
}
