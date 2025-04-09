// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "popupwindow.h"

#include <QTimer>
#include <QAccessibleEvent>

PopupWindow::PopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, FloatWidget, parent)
{
    setMargin(0);

    setBackgroundColor(QColor(235, 235, 235, 80));
    setBorderColor(QColor(235, 235, 235, 80));

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint);
    setShadowBlurRadius(20);
    setRadius(6);
    setShadowYOffset(2);
    setShadowXOffset(0);
    setArrowWidth(18);
    setArrowHeight(10);
}

PopupWindow::~PopupWindow()
{
}

void PopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    QAccessibleEvent event(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&event);

    if (!content->objectName().trimmed().isEmpty())
        setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void PopupWindow::toggle(const QPoint &pos)
{
    isVisible() ? hide() : show(pos);
}

void PopupWindow::show(const QPoint &pos)
{
    m_lastPoint = pos;
    if (getContent())
        getContent()->setVisible(true);
    DArrowRectangle::show(pos.x(), pos.y());
}

void PopupWindow::hide()
{
    if (getContent())
        getContent()->setVisible(false);
    Q_EMIT visibleChanged(false);
    DArrowRectangle::hide();
}

void PopupWindow::showEvent(QShowEvent *e)
{
    Q_EMIT visibleChanged(true);
    DArrowRectangle::showEvent(e);

    QTimer::singleShot(0, this, &PopupWindow::ensureRaised);
}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PopupWindow::enterEvent(QEnterEvent *e)
#else
void PopupWindow::enterEvent(QEvent *e)
#endif
{
    DArrowRectangle::enterEvent(e);

    QTimer::singleShot(0, this, &PopupWindow::ensureRaised);
}

bool PopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize)
        return false;

    // FIXME: ensure position move after global mouse release event
    // 目的是为了在content resize之后重新设置弹窗的位置和大小
    if (isVisible()) {
        QTimer::singleShot(0, this, [this] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint);
        });
    }

    return false;
}

void PopupWindow::ensureRaised()
{
    if (isVisible()) {
        QWidget *content = getContent();
        if (!content || !content->isVisible()) {
            this->setVisible(false);
        } else {
            raise();
        }
    }
}
