// SPDX-FileCopyrightText: 2011-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ABSTRACTFULLBACKGROUND_H
#define ABSTRACTFULLBACKGROUND_H

#include <QPointer>
#include <QScreen>

class AbstractFullBackgroundInterface
{
public:
    virtual void setScreen(QPointer<QScreen> screen, bool isVisible = true) = 0;
};

#endif // ABSTRACTFULLBACKGROUND_H