// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tipswidget.h"

DWIDGET_USE_NAMESPACE

TipsWidget::TipsWidget(QWidget *parent)
    : DArrowRectangle(DArrowRectangle::ArrowBottom, parent)
{
    setProperty("_d_radius_force", true); // 无特效模式时，让窗口圆角
    setShadowBlurRadius(20);
    setRadius(6);
    setShadowYOffset(2);
    setShadowXOffset(0);
    setArrowWidth(18);
    setArrowHeight(10);
}

void TipsWidget::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);
    DArrowRectangle::setContent(content);
}

void TipsWidget::show(int x, int y)
{
    m_lastPos = QPoint(x, y);
    DArrowRectangle::show(x, y);
}

bool TipsWidget::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize)
        return false;

    if (isVisible()) {
        QTimer::singleShot(10, this, [=] {
            if (isVisible())
                show(m_lastPos.x(), m_lastPos.y());
        });
    }

    return DArrowRectangle::eventFilter(o, e);
}
