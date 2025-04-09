// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIGHTERBACKGROUND_H
#define LIGHTERBACKGROUND_H

#include <QWidget>

#include "abstractfullbackgroundinterface.h"

class QPaintEvent;
class QScreen;

class LighterBackground : public QWidget, public AbstractFullBackgroundInterface
{
    Q_OBJECT
public:
    explicit LighterBackground(QWidget *content, QWidget *parent = nullptr);

    void setScreen(QPointer<QScreen> screen, bool isVisible = true) override;

public Q_SLOTS:
    void onAuthFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_content;

    bool m_useSolidBackground;
    bool m_showBlack;
};

#endif // LIGHTERBACKGROUND_H
