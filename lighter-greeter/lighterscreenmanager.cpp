// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lighterscreenmanager.h"
#include "abstractfullbackgroundinterface.h"

#include <QApplication>
#include <QScreen>
#include <QDebug>

LighterScreenManager::LighterScreenManager(std::function<QWidget *(QScreen *)> function, QObject *parent)
    : QObject(parent)
    , m_registerFun(function)
{
    Q_ASSERT(m_registerFun);

    connect(qApp, &QGuiApplication::screenAdded, this, &LighterScreenManager::screenCountChanged);
    connect(qApp, &QGuiApplication::screenRemoved, this, &LighterScreenManager::screenCountChanged);

    screenCountChanged();
}

void LighterScreenManager::screenCountChanged()
{
    QList<ScreenPtr> screens = m_screenContents.values();

    // 找到过期的screen指针
    QList<ScreenPtr> screens_to_remove;
    for (const auto &s : screens) {
        if (!qApp->screens().contains(s))
            screens_to_remove.append(s);
    }

    // 找出新增的screen指针
    QList<ScreenPtr> screens_to_add;
    for (const auto &s : qApp->screens()) {
        if (!screens.contains(s)) {
            screens_to_add.append(s);
        }
    }

    // 释放过期的资源
    for (auto &s : screens_to_remove) {
        disconnect(s);
        auto backgroundFrame = m_screenContents.key(s);
        if (backgroundFrame) {
            // @note 创建的界面并不释放，后面可以继续复用,如果为了内存考虑，也可以启动一个定时器，在一分钟后释放(释放后要从map中remove)
            backgroundFrame->setVisible(false);
        }
        s = nullptr;
    }

    // 处理新增的资源
    for (const auto &s : screens_to_add) {
        s->setOrientationUpdateMask(Qt::PrimaryOrientation
                                    | Qt::LandscapeOrientation
                                    | Qt::PortraitOrientation
                                    | Qt::InvertedLandscapeOrientation
                                    | Qt::InvertedPortraitOrientation);

        // 显示器信息发生任何变化时，都应该重新刷新一次显示情况
        connect(s, &QScreen::geometryChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::availableGeometryChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::physicalSizeChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::physicalDotsPerInchChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::logicalDotsPerInchChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::virtualGeometryChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::primaryOrientationChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::orientationChanged, this, &LighterScreenManager::handleScreenChanged);
        connect(s, &QScreen::refreshRateChanged, this, &LighterScreenManager::handleScreenChanged);

        auto findContentNotUsed = [ = ](QMap<QWidget *, ScreenPtr> map) ->QWidget * {
            QMapIterator<QWidget *, ScreenPtr> it(map);
            while (it.hasNext())
            {
                it.next();
                if (!it.value() && it.key()) {
                    return it.key();
                }
            }
            return nullptr;
        };

        // try to find a content frame not used now
        QWidget *content = findContentNotUsed(m_screenContents);

        // or create new content frame
        if (!content) {
            content = m_registerFun(s);
        }

        content->setVisible(true);

        m_screenContents.insert(content, s);
    }
}

void LighterScreenManager::handleScreenChanged()
{
    for (QWidget *w : m_screenContents.keys()) {
        auto inter = dynamic_cast<AbstractFullBackgroundInterface *>(w);
        if (inter && m_screenContents.value(w)) {
            inter->setScreen(m_screenContents.value(w));
        }
    }
}
