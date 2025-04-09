// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QWindow>

#include "lighterbackground.h"
#include "dconfig_helper.h"
#include "constants.h"

LighterBackground::LighterBackground(QWidget *content, QWidget *parent)
    : QWidget(parent)
    , m_content(content)
    , m_useSolidBackground(DConfigHelper::instance()->getConfig( "useSolidBackground", false).toBool())
    , m_showBlack(false)
{
    Q_ASSERT(m_content);

    this->installEventFilter(this);

#ifndef QT_DEBUG
    if (!qgetenv("XDG_SESSION_TYPE").startsWith("wayland")) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    } else {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::Window);

        setAttribute(Qt::WA_NativeWindow); // 创建窗口 handle
        create();
        // onScreenDisplay 低于override，高于tooltip，希望显示在锁屏上方的界面，均需要调整层级为onScreenDisplay或者override
        windowHandle()->setProperty("_d_dwayland_window-type", "onScreenDisplay");
    }
#endif

}

void LighterBackground::setScreen(QPointer<QScreen> screen, bool isVisible)
{
    Q_UNUSED(isVisible);

    setGeometry(screen->geometry());
    show();

    if (screen == qApp->primaryScreen() && !m_showBlack) {
        activateWindow();

        m_content->hide();
        m_content->setParent(this);
        m_content->resize(this->size());
        m_content->show();
    }
}

void LighterBackground::onAuthFinished()
{
    m_showBlack = true;

    // 强制刷新，从而显示黑色画面，否则在lightdm开启会话时可能不会立即显示黑色
    repaint();
}

void LighterBackground::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));

    // 绘制纯黑色画面，提升体验
    if (m_showBlack) {
        painter.fillRect(trueRect, Qt::black);
        return;
    }

    if (!QFile("/usr/share/backgrounds/default_background.jpg").exists() || m_useSolidBackground) {
        painter.fillRect(trueRect, QColor(DDESESSIONCC::SOLID_BACKGROUND_COLOR));
    } else {
        // cache the pixmap
        static QPixmap background = QPixmap("/usr/share/backgrounds/default_background.jpg").scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // 缓存图片大小和当前屏幕大小不一致时，需要调整尺寸
        if (background.size() != this->size()) {
            background.scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        painter.drawPixmap(trueRect, background, QRect(trueRect.topLeft(), trueRect.size() * background.devicePixelRatioF()));
    }
}

bool LighterBackground::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this && !m_showBlack
            && (event->type() == QEvent::Enter)) {
        activateWindow();

        m_content->hide();
        m_content->setParent(this);
        m_content->resize(this->size());
        m_content->show();
    }

    if (watched == this && !m_showBlack
            && event->type() == QEvent::Resize) {
        auto screen = qApp->screenAt(m_content->geometry().center());
        if (screen) {
            m_content->resize(this->size());
        }
    }

    return QWidget::eventFilter(watched, event);
}
