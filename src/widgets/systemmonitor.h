// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include "actionwidget.h"

#include <QLabel>
#include <QWidget>

class SystemMonitor : public ActionWidget
{
    Q_OBJECT
public:

    explicit SystemMonitor(QWidget *parent = nullptr);

private:
    void initUI();

private:
    QLabel *m_icon;
    QLabel *m_text;
};

#endif // SYSTEMMONITOR_H
