/*
  * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
  *
  * Author:     dengbo <dengbo@uniontech.com>
  *
  * Maintainer: dengbo <dengbo@uniontech.com>
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

#ifndef STATEBUTTON_H
#define STATEBUTTON_H

#include <QWidget>

class StateButton : public QWidget
{
    Q_OBJECT

public:
    enum Type {
        Check,
        Fork
    };

public:
    explicit StateButton(QWidget *parent = nullptr);

signals:
    void click();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawCheck(QPainter &painter, QPen &pen, int radius);

private:
    Type m_type;
};

#endif // STATEBUTTON_H
