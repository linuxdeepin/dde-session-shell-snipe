// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYBOARDICONWIDGET_H
#define KEYBOARDICONWIDGET_H

#include <QWidget>

class KeyboardIconWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KeyboardIconWidget(QWidget *parent = nullptr);

    void setIconPath(const QString &iconPath);

signals:
    void topLevelWidgetHided();
    void iconWidgetHided();
    void clicked(QWidget *parent);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);

private:
    QString m_iconPath;
};

#endif // KEYBOARDICONWIDGET_H
