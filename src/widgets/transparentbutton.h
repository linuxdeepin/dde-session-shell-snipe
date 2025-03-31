// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRANSPARENTBUTTON_H
#define TRANSPARENTBUTTON_H

#include <DFloatingButton>

DWIDGET_USE_NAMESPACE

class TransparentButton : public DFloatingButton
{
    Q_OBJECT

public:
    explicit TransparentButton(QWidget *parent = nullptr);

    void setColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
};

#endif // TRANSPARENTBUTTON_H
