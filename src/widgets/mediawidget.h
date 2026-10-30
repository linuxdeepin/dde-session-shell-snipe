// SPDX-FileCopyrightText: 2011-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEDIAWIDGET_H
#define MEDIAWIDGET_H

#include <dmpriscontrol.h>

#include <QWidget>

DWIDGET_USE_NAMESPACE

class MediaWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MediaWidget(QWidget *parent = nullptr);
    void initMediaPlayer();

private Q_SLOTS:
    void changeVisible();

private:
    void initUI();
    void initConnect();

private:
    DMPRISControl *m_dmprisWidget;
};

#endif // MEDIAWIDGET_H