// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "multiscreenmanager.h"
#include "fullscreenbackground.h"
#include "constants.h"

#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "dbusconstant.h"

MultiScreenManager::MultiScreenManager(QObject *parent)
    : QObject(parent)
    , m_registerFunction(nullptr)
    , m_raiseContentFrameTimer(new QTimer(this))
    , m_systemDisplay(new SystemDisplayInter(DSS_DBUS::systemDisplayService, DSS_DBUS::systemDisplayPath, QDBusConnection::systemBus(), this))
    , m_isCopyMode(false)
{
    connect(qApp, &QGuiApplication::screenAdded, this, &MultiScreenManager::onScreenAdded, Qt::DirectConnection);
    connect(qApp, &QGuiApplication::screenRemoved, this, &MultiScreenManager::onScreenRemoved, Qt::DirectConnection);

    // 在sw平台存在复制模式显示问题，使用延迟来置顶一个Frame
    m_raiseContentFrameTimer->setInterval(50);
    m_raiseContentFrameTimer->setSingleShot(true);

    connect(m_raiseContentFrameTimer, &QTimer::timeout, this, &MultiScreenManager::raiseContentFrame);
    connect(m_systemDisplay, &SystemDisplayInter::ConfigUpdated, this, &MultiScreenManager::onDisplayModeChanged);
    if (m_systemDisplay->isValid()) {
        m_isCopyMode = (COPY_MODE == getDisplayModeByConfig(m_systemDisplay->GetConfig()));
    }
}

void MultiScreenManager::register_for_multi_screen(std::function<QWidget *(QScreen *, int)> function)
{
    m_registerFunction = function;
    qCInfo(DDE_SHELL) << "Copy mode:" << m_isCopyMode;

    int maxAttempts = 0;
    while (true) {
        if (qApp->screens().count() < 1) {
            // 没有屏幕数据延时2s再取一次
            QThread::sleep(2);
            qWarning(" #### qApp->screens : %d, repeat times : %d .", qApp->screens().count(),  maxAttempts++);
            if (maxAttempts == 50) {
                break;
            }
        } else {
            break;
        }
    }

    // update all screen
    if (m_isCopyMode) {
        if (!qApp->screens().isEmpty()) {
            QScreen *validScreen = nullptr;
            for (QScreen *screen : qApp->screens()) {
                // 留下一个可用的屏幕
                if (!screen->name().isEmpty()) {
                    validScreen = screen;
                    break;
                }
            }
            if (!validScreen)
                return;
            onScreenAdded(validScreen);
        }
    } else {
        for (QScreen *screen : qApp->screens()) {
            // 当greeter刚起来处理第一个屏幕时，第二个屏幕被拔掉，这时第二个屏幕指针被释放，不应该在继续处理，否则会导致崩溃
            if (!qApp->screens().contains(screen)) {
                qCWarning(DDE_SHELL) << "Screen pointer has been released";
                continue;
            }
            onScreenAdded(screen);
        }
    }
}

void MultiScreenManager::startRaiseContentFrame(const bool visible)
{
    if (visible) {
        m_raiseContentFrameTimer->start();
    }
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
    qCInfo(DDE_SHELL) << "Is copy mode:" << m_isCopyMode << ", screen: " << screen;

    // 虚拟屏幕不处理
    if (screen.isNull() || (screen->name().isEmpty() && (screen->geometry().width() == 0 || screen->geometry().height() == 0))) {
        return;
    }

    if (!m_registerFunction) {
        return;
    }

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
        // wayland下没有屏幕时，容易导致qt崩溃
        if (!QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive)) {
            w->installEventFilter(this);
        }
    } else if (w) {
        w->deleteLater();
    }

    startRaiseContentFrame();
}

void MultiScreenManager::onScreenRemoved(QPointer<QScreen> screen)
{
    qDebug() << "Is copy mode: " << m_isCopyMode << ", screen: " << screen;
    // 虚拟屏幕不处理
    if (screen.isNull() || (screen->name().isEmpty() && (screen->geometry().width() == 0 || screen->geometry().height() == 0))) {
        qCWarning(DDE_SHELL) << "Remove skipped, screen error";
        return;
    }

    if (!m_registerFunction) {
        return;
    }

    if (m_frames.contains(screen)) {
        if (m_isCopyMode) {
            QWidget *frame = m_frames[screen];
            m_frames.remove(screen);
            // 如果此时m_frames为空则其它的屏幕继续使用此frame，不重新创建
            QScreen *validScreen = nullptr;
            if (!qApp->screens().isEmpty() && m_frames.isEmpty()) {
                for (QScreen *screen : qApp->screens()) {
                    // 留下一个可用的屏幕
                    if (!screen->name().isEmpty()) {
                        validScreen = screen;
                        break;
                    }
                }
                if (!validScreen) {
                    frame->deleteLater();
                    return;
                }
                // 更新frame绑定的屏幕
                m_frames[validScreen] = frame;
                AbstractFullBackgroundInterface *fullScreenFrameInterface = dynamic_cast<AbstractFullBackgroundInterface*>(frame);
                if (fullScreenFrameInterface) {
                    fullScreenFrameInterface->setScreen(validScreen, true);
                }
            } else {
                frame->deleteLater();
            }
        }else {
            m_frames[screen]->deleteLater();
            m_frames.remove(screen);
        }
        updateFrame();
    }

    startRaiseContentFrame();
}

void MultiScreenManager::updateFrame()
{
    for (const auto screen : m_frames.keys()) {
        FullScreenBackground *fullScreenFrame = qobject_cast<FullScreenBackground*>(m_frames[screen]);
        if (fullScreenFrame) {
            fullScreenFrame->setScreen(screen, true);
        }
    }
}

void MultiScreenManager::raiseContentFrame()
{
    for (auto it = m_frames.constBegin(); it != m_frames.constEnd(); ++it) {
        if (it.value() && it.value()->property("contentVisible").toBool()) {
            it.value()->raise();
            if (QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive)) {
                it.value()->activateWindow();
                it.value()->setFocus();
            }

            // bug189309, 重启或注锁后，后端屏幕变化通知太晚
            // 触发move事件处理content是否显示
            it.value()->updateGeometry();

            return;
        }
    }
}

void MultiScreenManager::onDisplayModeChanged(const QString &)
{
    m_isCopyMode = (COPY_MODE == getDisplayModeByConfig(m_systemDisplay->GetConfig()));

    qCInfo(DDE_SHELL) << "Display mode changed, copy mode: " << m_isCopyMode << ", screen size: " << qApp->screens().size();
    if (m_isCopyMode) {
        QScreen *validScreen = nullptr;
        for (QScreen *screen : qApp->screens()) {
            if (screen->name().isEmpty())
                continue;
            if (!validScreen)
                validScreen = screen;
            // 留下一个可用的屏
            if (screen != validScreen)
                onScreenRemoved(screen);
        }
    } else {
        for (QScreen *screen : qApp->screens()) {
            if (!m_frames.contains(screen))
                onScreenAdded(screen);
        }
    }
}

void MultiScreenManager::checkLockFrameLocation()
{
    for (QScreen *screen : m_frames.keys()) {
        if (screen) {
            qCInfo(DDE_SHELL) << "Check lock frame location, screen:" << screen << ", location:" << screen->geometry()
                       << ", lockframe:" << m_frames.value(screen) << ", location:" << m_frames.value(screen)->geometry();
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
