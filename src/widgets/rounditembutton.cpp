// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QObject>
#include <QSvgRenderer>
#include "rounditembutton.h"

#include <DFontSizeManager>

DWIDGET_USE_NAMESPACE

const int iconWidthHeight = 74;
const int iconTextGap = 11;

RoundItemButton::RoundItemButton(QWidget *parent)
    : RoundItemButton("", parent)
{

}

RoundItemButton::RoundItemButton(const QString &text, QWidget *parent)
    : QAbstractButton(parent)
    , m_text(text)
{
    initUI();
    initConnect();
}

RoundItemButton::~RoundItemButton()
{
}

void RoundItemButton::setDisabled(bool disabled)
{
    if (!disabled)
        updateState(Normal);
    else
        updateState(Disabled);

    QAbstractButton::setDisabled(disabled);
}

void RoundItemButton::setChecked(bool checked)
{
    if (checked)
        updateState(Checked);
    else
        updateState(Normal);
}

void RoundItemButton::setText(const QString &text)
{
    m_text = text;

    update();
}

void RoundItemButton::initConnect()
{
    connect(this, &RoundItemButton::stateChanged, this, &RoundItemButton::setState, Qt::DirectConnection);
    connect(this, &RoundItemButton::stateChanged, this, &RoundItemButton::updateIcon);
    connect(this, &RoundItemButton::stateChanged, this, static_cast<void (RoundItemButton::*)()>(&RoundItemButton::update));
    connect(this, &RoundItemButton::iconChanged, this, &RoundItemButton::updateIcon);
    connect(this, &RoundItemButton::toggled, this, &RoundItemButton::setChecked);
}

void RoundItemButton::initUI()
{
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(144, 230);
    setCheckable(true);
    DFontSizeManager::instance()->bind(this, DFontSizeManager::T5);
}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void RoundItemButton::enterEvent(QEnterEvent* event)
#else
void RoundItemButton::enterEvent(QEvent* event)
#endif
{
    Q_UNUSED(event)

    if (m_state == Disabled)
        return;

    if (m_state == Normal && isEnabled()) {
        updateState(Hover);
    }

    //    emit signalManager->setButtonHover(m_itemText->text());
}

void RoundItemButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)

    if (m_state == Disabled)
        return;

    if (m_state == Checked)
        return;

    updateState(Normal);
}

void RoundItemButton::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event)

    updateState(Pressed);
}

void RoundItemButton::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e)

    if (m_state == Checked)
        updateState(Hover);
    else
        updateState(Pressed);

    if (m_state != Disabled)
        emit clicked();
}

void RoundItemButton::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);

    const int padding = 10;
    const int lineHeight = fontMetrics().height() + 10;

    // 计算文本行数
    QStringList textList;
    QString str = m_text;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    while (fontMetrics().horizontalAdvance(str) > width() - 20 && str.length() > 0) {
#else
    while (fontMetrics().width(str) > width() - 20 && str.length() > 0) {
#endif
        int lastSpacePos = str.lastIndexOf(" ");
        if (lastSpacePos == -1) {
            str = fontMetrics().elidedText(str, Qt::ElideRight, width() - 2 * padding);
        } else {
            str = m_text.left(lastSpacePos);
        }
    }
    textList.append(str);

    if (str.length() != m_text.length() && m_text.contains(str)) {
        textList.append(fontMetrics().elidedText(m_text.mid(m_text.indexOf(str) + str.length()), Qt::ElideRight, width() - 20));
    }
    // 计算图标绘制的区域
    QRect iconRect;
    iconRect.setX((width() - iconWidthHeight) / 2);
    iconRect.setY((height() - 2 * lineHeight- iconWidthHeight) / 2);
    iconRect.setWidth(iconWidthHeight);
    iconRect.setHeight(iconWidthHeight);

    // 计算文本绘制的区域
    int textWidth = 0;
    for (auto text : textList) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        textWidth = qMax(textWidth, fontMetrics().horizontalAdvance(text));
#else
        textWidth = qMax(textWidth, fontMetrics().width(text));
#endif
    }
    QRect textRect;
    textRect.setX((width() - qMin(width(), textWidth + 2 * padding)) / 2);
    textRect.setY(iconRect.bottom() + iconTextGap);
    textRect.setWidth(qMin(width(), textWidth + 2 * padding));
    textRect.setHeight(textList.size() * lineHeight);


    if (m_state == Checked) {
        QColor color(Qt::white);
        color.setAlphaF(0.03);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 绘制图标背景
        QRect itemIconRect = iconRect.marginsRemoved(QMargins(m_penWidth, m_penWidth, m_penWidth, m_penWidth));
        painter.drawEllipse(itemIconRect);
        // 绘制文本背景
        QRect itemTextRect = textRect.marginsRemoved(QMargins(m_penWidth, m_penWidth, m_penWidth, m_penWidth));
        painter.drawRoundedRect(itemTextRect, m_rectRadius, m_rectRadius);

        QPen pen;
        QColor penColor(Qt::white);
        penColor.setAlphaF(0.5);
        pen.setColor(penColor);
        pen.setWidth(m_penWidth * 3);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 绘制图标区域边框
        QRect iconBackgroundRect(iconRect.marginsRemoved(QMargins(m_penWidth, m_penWidth, m_penWidth, m_penWidth)));
        painter.drawEllipse(iconBackgroundRect);

        // 绘制文本区域边框
        pen.setWidth(m_penWidth * 2);
        painter.setPen(pen);
        QRect textBackgroundRect(textRect.marginsRemoved(QMargins(m_penWidth, m_penWidth, m_penWidth, m_penWidth)));
        painter.drawRoundedRect(textBackgroundRect, m_rectRadius, m_rectRadius);
    } else if (m_state == Hover) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 127));
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 绘制鼠标选中的白色背景
        painter.drawRoundedRect(textRect, m_rectRadius, m_rectRadius);
        painter.drawEllipse(iconRect);
    } else if (m_state == Normal) {
        // 绘制鼠标选中的白色背景
        painter.setPen(Qt::NoPen);
        QColor color(Qt::white);
        color.setAlphaF(0.15);
        painter.setBrush(color);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(iconRect);
    }

    // 图标
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QSvgRenderer renderer(m_currentIcon);
    if (!isEnabled()) {
        painter.setOpacity(0.4);
    }
    renderer.render(&painter, iconRect);

    // 绘制文本内容
    for (int i = 0; i < textList.size(); ++i) {
        QRect lineRect;
        lineRect.setX(textRect.x());
        lineRect.setY(textRect.y() + i * lineHeight);
        lineRect.setWidth(textRect.width());
        lineRect.setHeight(lineHeight);
        if (m_state == Checked) {
            painter.setPen(Qt::white);
        } else if (m_state == Hover) {
            painter.setPen(Qt::black);
        } else if (m_state == Normal) {
            painter.setPen(Qt::white);
        }

        if (!isEnabled())
            painter.setPen(Qt::gray);

        painter.drawText(lineRect, Qt::AlignCenter, textList.at(i));
    }

    // 绘制文字旁的小红点
    if (m_redPointVisible) {
        QRect textRt(textRect.x(), textRect.y(), textRect.width(), textList.size() * lineHeight);
        const int spaceToText = 4;     //文字和红点的距离
        const int radius = 3;          //红点的半径
        QPoint centerOfCircle=textRt.topRight() + QPoint(spaceToText, textRt.height() / 2);
        painter.setBrush(QBrush(Qt::red));
        painter.setPen(Qt::red);
        painter.drawEllipse(centerOfCircle, radius, radius);
    }
}

void RoundItemButton::updateIcon()
{
    switch (m_state)
    {
    case Disabled:  /* show normal pic */
    case Normal:    m_currentIcon = m_normalIcon;  break;
    case Default:
    case Hover:     m_currentIcon = m_hoverIcon;   break;
    case Checked:   m_currentIcon = m_normalIcon;  break;
    case Pressed:   m_currentIcon = m_pressedIcon; break;
    }

    update();
}

void RoundItemButton::updateState(const RoundItemButton::State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }

    QAbstractButton::setChecked(m_state == Checked);
    return updateIcon();
}

void RoundItemButton::setNormalPic(const QString &path)
{
    m_normalIcon = path;

    updateIcon();
}

void RoundItemButton::setHoverPic(const QString &path)
{
    m_hoverIcon = path;

    updateIcon();
}

void RoundItemButton::setPressPic(const QString &path)
{
    m_pressedIcon = path;

    updateIcon();
}

void RoundItemButton::setRedPointVisible(bool visible)
{
    m_redPointVisible = visible;

    update();
}
