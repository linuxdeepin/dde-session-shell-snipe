/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dlineeditex.h"

#include <QPainter>
#include <QEvent>

DLineEditEx::DLineEditEx(QWidget *parent)
    : DLineEdit(parent)
{
#if 0 // FIXME:不生效,改为在eventFilter过滤InputMethodQuery事件
    // 在锁屏或登录界面，输入类型的控件不要唤出输入法
    setAttribute(Qt::WA_InputMethodEnabled, false);
    this->lineEdit()->setAttribute(Qt::WA_InputMethodEnabled, false);
#endif

    this->lineEdit()->installEventFilter(this);
    this->installEventFilter(this);
}

//重写QLineEdit paintEvent函数，实现当文本设置居中后，holderText仍然显示的需求
void DLineEditEx::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (lineEdit()->hasFocus() && lineEdit()->alignment() == Qt::AlignCenter
            && !lineEdit()->placeholderText().isEmpty() && lineEdit()->text().isEmpty()) {
        QPainter pa(this);
        QPalette pal = palette();
        QColor col = pal.text().color();
        col.setAlpha(128);
        QPen oldpen = pa.pen();
        pa.setPen(col);
        QTextOption option;
        option.setAlignment(Qt::AlignCenter);
        pa.drawText(lineEdit()->rect(), lineEdit()->placeholderText(), option);
    }
}

bool DLineEditEx::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    // 禁止输入法
    if ((watched == this || watched == this->lineEdit()) && event->type() == QEvent::InputMethodQuery) {
        return true;
    }

    return QWidget::eventFilter(watched,event);
}
