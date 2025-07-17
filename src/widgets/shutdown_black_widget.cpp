// SPDX-FileCopyrightText: 2020 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shutdown_black_widget.h"
#include <signal.h>

#include <QPainter>
#include <QApplication>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDebug>
#include <QGSettings>
#include <QTimer>

bool onPreparingForShutdown() {
    QDBusInterface iface(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        QDBusConnection::systemBus()
    );
    QVariant preparingForShutdown = iface.property("PreparingForShutdown");

    if (preparingForShutdown.isValid()) {
        bool isPreparing = preparingForShutdown.toBool();
        qDebug() << "Preparing for shutdown property value:" << isPreparing;
        return isPreparing;
    } else {
        qWarning() << "Failed to retrieve preparing for shutdown property, the property is invalid";
    }
    return false;
}

void handleSIGTERM(int signal) {
    qInfo() << "handleSIGTERM: " << signal;

    bool bShutdown = onPreparingForShutdown();
    qInfo() << "Whether preparing for shutdown: " << bShutdown;
    if (bShutdown) {
        QTimer::singleShot(2500, qApp, SLOT(quit()));
    } else {
        QTimer time;
        time.start(1000);
        QObject::connect(&time, &QTimer::timeout, [&] {
            bool bShutdown = onPreparingForShutdown();
            qInfo() << "Whether preparing for shutdown: " << bShutdown;
            if (bShutdown) {
                time.stop();
                QTimer::singleShot(2000, qApp, SLOT(quit()));
                return;
            } else {
                qInfo() << " Get org.freedesktop.login1.Manager PreparingForShutdown again.";
            }
        });
    }
}

ShutdownBlackWidget::ShutdownBlackWidget(QWidget *parent)
    : QWidget(parent)
    , m_cursor(cursor())
{
    signal(SIGTERM, handleSIGTERM);
    setObjectName(QStringLiteral("ShutdownBlackWidget"));
    setAccessibleName(QStringLiteral("ShutdownBlackWidget"));
    setVisible(false);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);

    QCursor cursor(Qt::BlankCursor);
    this->setCursor(cursor);

    installEventFilter(this);
}

void ShutdownBlackWidget::setBlackMode(const bool isBlackMode)
{
    qInfo() << "ShutdownBlackWidget setBlackMode : " << isBlackMode;
    setVisible(isBlackMode);
    if (isBlackMode) {
        show();
    }
#ifndef QT_DEBUG
    setCursor(isBlackMode ? Qt::BlankCursor : m_cursor);
#endif
}

void ShutdownBlackWidget::setShutdownMode(const bool isBlackMode)
{
    setVisible(isBlackMode);
}

void ShutdownBlackWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QRect rect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));
    painter.fillRect(rect, Qt::black);

    QWidget::paintEvent(event);
}

void ShutdownBlackWidget::showEvent(QShowEvent *event)
{
    raise();
    grabMouse();
    grabKeyboard();
    QWidget::showEvent(event);
}

bool ShutdownBlackWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick ||
        event->type() == QEvent::MouseMove ||
        event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease) {
        qDebug() << "### All Mouse and Keyboard Event blocked!";

        // 返回 true 表示事件已被处理，不再继续传递
        return true;
    }

    return QWidget::eventFilter(watched, event);
}
