// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIPSWIDGET_H
#define TIPSWIDGET_H

#include <DArrowRectangle>

class TipsWidget : public Dtk::Widget::DArrowRectangle
{
public:
    explicit TipsWidget(QWidget *parent = nullptr);
    void setContent(QWidget *content);

public slots:
    void show(int x, int y) override;

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

protected:
    QPoint m_lastPos;
};

#endif // TIPSWIDGET_H
