// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "multiscreenmanager.h"
#include "fullscreenbackground.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>
#include <QDesktopWidget>
#include <QJsonDocument>
#include <QJsonObject>

MultiScreenManager::MultiScreenManager(QObject *parent)
    : QObject(parent)
    , m_registerFunction(nullptr)
    , m_raiseContentFrameTimer(new QTimer(this))
    , m_systemDisplay(new SystemDisplayInter("com.deepin.system.Display", "/com/deepin/system/Display", QDBusConnection::systemBus(), this))
    , m_isCopyMode(false)
{
    connect(qApp, &QGuiApplication::screenAdded, this, &MultiScreenManager::dealScreenAdded, Qt::DirectConnection);
    connect(qApp, &QGuiApplication::screenRemoved, this, &MultiScreenManager::dealScreenRemoved, Qt::DirectConnection);

    // 锁屏采用直接连接的方式处理屏幕插拔事件，会有一定耗时，当屏幕移除消息到来，屏幕被移除，而上层尚未处理完毕，导致 qtwayland 崩溃
    // 此处为了规避qt的崩溃问题，实属无奈之举
    connect(this, &MultiScreenManager::screenRemoveSignal, this, &MultiScreenManager::onScreenAdded, Qt::QueuedConnection);
    connect(this, &MultiScreenManager::screenAddSignal, this, &MultiScreenManager::onScreenRemoved, Qt::QueuedConnection);

    // 在sw平台存在复制模式显示问题，使用延迟来置顶一个Frame
    m_raiseContentFrameTimer->setInterval(50);
    m_raiseContentFrameTimer->setSingleShot(true);

    connect(m_raiseContentFrameTimer, &QTimer::timeout, this, &MultiScreenManager::raiseContentFrame);
    connect(m_systemDisplay, &SystemDisplayInter::ConfigUpdated, this, &MultiScreenManager::onDisplayModeChanged);
    if (m_systemDisplay->isValid()) {
        m_isCopyMode = (COPY_MODE == getDisplayModeByConfig(m_systemDisplay->GetConfig()));
    }
}

void MultiScreenManager::dealScreenAdded(QPointer<QScreen> screen)
{
    qInfo() << Q_FUNC_INFO << ", is copy mode: " << m_isCopyMode << ", screen: " << screen;

    // 虚拟屏幕不处理
    if (screen.isNull() || (screen->name().isEmpty() && (screen->geometry().width() == 0 || screen->geometry().height() == 0))) {
        return;
    }

    if (!m_registerFunction) {
        return;
    }
    Q_EMIT screenAddSignal(screen);
}

void MultiScreenManager::dealScreenRemoved(QPointer<QScreen> screen)
{
    qInfo() << Q_FUNC_INFO << ", is copy mode: " << m_isCopyMode << ", screen: " << screen;

    // 虚拟屏幕不处理
    if (screen.isNull() || (screen->name().isEmpty() && (screen->geometry().width() == 0 || screen->geometry().height() == 0))) {
        return;
    }

    if (!m_registerFunction) {
        return;
    }
    Q_EMIT screenRemoveSignal(screen);
}

void MultiScreenManager::register_for_mutil_screen(std::function<QWidget *(QScreen *, int)> function)
{
    m_registerFunction = function;
    qInfo() << Q_FUNC_INFO << ", is copy mode: " << m_isCopyMode;
    // update all screen
    if (m_isCopyMode) {
        if (!qApp->screens().isEmpty())
            onScreenAdded(qApp->screens().first());
    } else {
        for (QScreen *screen : qApp->screens()) {
            onScreenAdded(screen);
        }
    }
}

void MultiScreenManager::startRaiseContentFrame()
{
    m_raiseContentFrameTimer->start();
}

bool MultiScreenManager::eventFilter(QObject *watched, QEvent *event)
{
    // 捕获lockframe窗口显示事件
    if (event->type() == QEvent::Show) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && m_frames.values().contains(widget)) {
            // 显示的时候获取窗口位置信息（调试）
            QTimer::singleShot(0, this, &MultiScreenManager::checkLockFrameLocation);
        }
    }

    return QObject::eventFilter(watched, event);
}

void MultiScreenManager::onScreenAdded(QPointer<QScreen> screen)
{
    QWidget* w = nullptr;
    if (m_isCopyMode) {
        // 如果m_frames不为空则直接退出
        if (m_frames.isEmpty()) {
            w = m_registerFunction(screen, m_frames.size());
        }
    } else {
        w = m_registerFunction(screen, m_frames.size());
    }

    // 创建全屏窗口的时间较长，可能在此期间屏幕已经被移除了且指针被析构了（手动操作比较难出现，如果显卡或驱动有问题则会出现），
    // 如果指针为空，则不加入Map中，并析构创建的全屏窗口。
    if (w && !screen.isNull()) {
        m_frames[screen] = w;
        w->installEventFilter(this);
    } else if (w) {
        w->deleteLater();
    }

    startRaiseContentFrame();
}

void MultiScreenManager::onScreenRemoved(QPointer<QScreen> screen)
{
    if (m_frames.contains(screen)) {
        if (m_isCopyMode) {
            QWidget *frame = m_frames[screen];
            m_frames.remove(screen);
            // 如果此时m_frames为空则其它的屏幕继续使用此frame，不重新创建
            if (!qApp->screens().isEmpty() && m_frames.isEmpty()) {
                QScreen *screen = qApp->screens().first();
                // 更新frame绑定的屏幕
                m_frames[screen] = frame;
                FullscreenBackground *fullScreenFrame = qobject_cast<FullscreenBackground*>(frame);
                if (fullScreenFrame) {
                    fullScreenFrame->setScreen(screen, true);
                }
            } else {
                frame->deleteLater();
            }
        }else {
            m_frames[screen]->deleteLater();
            m_frames.remove(screen);
        }
    }

    startRaiseContentFrame();
}

void MultiScreenManager::raiseContentFrame()
{
    for (auto it = m_frames.constBegin(); it != m_frames.constEnd(); ++it) {
        if (it.value()->property("contentVisible").toBool()) {
            it.value()->raise();
            if (QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive)) {
                it.value()->setFocus();
            }
            return;
        }
    }
}

void MultiScreenManager::onDisplayModeChanged(const QString &)
{
    m_isCopyMode = (COPY_MODE == getDisplayModeByConfig(m_systemDisplay->GetConfig()));

    qInfo() << Q_FUNC_INFO << ", copy mode: " << m_isCopyMode << ", screen size: " << qApp->screens().size();
    if (m_isCopyMode) {
        for (QScreen *screen : qApp->screens()) {
            // 留下一个屏即可
            if (screen != qApp->screens().first())
                onScreenRemoved(screen);
        }
    } else {
        for (QScreen *screen : qApp->screens()) {
            if (!m_frames[screen])
                onScreenAdded(screen);
        }
    }
}

void MultiScreenManager::checkLockFrameLocation()
{
    for (QPointer<QScreen> screen : m_frames.keys()) {
        if (!screen.isNull() && !m_frames.value(screen).isNull()) {
            qInfo() << Q_FUNC_INFO << ", screen:" << screen << " location:" << screen->geometry()
                       << " lockframe:" << m_frames.value(screen) << " location:" << m_frames.value(screen)->geometry();
        } else {
            delete m_frames[screen];
            m_frames[screen] = nullptr;
            m_frames.remove(screen);
        }
    }
}

int MultiScreenManager::getDisplayModeByConfig(const QString &config) const
{
    if (config.isEmpty())
        return EXTENDED_MODE;

    const QJsonObject &rootObj = QJsonDocument::fromJson(config.toUtf8()).object();
    if (rootObj.contains("Config")) {
        const QJsonObject &configObj = rootObj.value("Config").toObject();
        if (configObj.contains("DisplayMode")) {
            return configObj.value("DisplayMode").toInt();
        }
    }

    return EXTENDED_MODE;
}
