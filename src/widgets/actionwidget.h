// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTIONWIDGET_H
#define ACTIONWIDGET_H

#include <QLabel>
#include <QWidget>

class ActionWidget : public QWidget
{
    Q_OBJECT
public:
    enum State {
        Enter,
        Leave,
        Press,
        Release
    };

    explicit ActionWidget(QWidget *parent = nullptr);

    inline State state() { return m_state; }
    void setState(const State state);

    void setRadius(int radius) { m_radius = radius; }

signals:
    void requestAction();
    void mouseEnter();
    void mouseLeave();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *e) override;
#else
    void enterEvent(QEvent *e) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    State m_state;
    int m_radius;
};

#endif // ACTIONWIDGET_H
