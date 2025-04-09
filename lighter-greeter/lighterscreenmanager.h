// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIGHTERSCREENMANAGER_H
#define LIGHTERSCREENMANAGER_H

#include <QObject>
#include <QScreen>
#include <QMap>
#include <QWidget>

class LighterScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit LighterScreenManager(std::function<QWidget *(QScreen *)> function, QObject *parent = nullptr);

    typedef QPointer<QScreen> ScreenPtr;

Q_SIGNALS:
    void copyModeChanged(bool isCopyMode);

public Q_SLOTS:
    void screenCountChanged();
    void handleScreenChanged();

private:
    std::function<QWidget *(QScreen *)> m_registerFun;
    QMap<QWidget *, ScreenPtr> m_screenContents;
};

#endif // LIGHTERSCREENMANAGER_H
