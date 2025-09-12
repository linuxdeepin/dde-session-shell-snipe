// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GESTUREPANNEL_H
#define GESTUREPANNEL_H

#include <QObject>
#include <QWidget>

#include <QPoint>
#include <QVector>
#include <QPainter>
#include <QMap>
#include <QRectF>
#include <QStateMachine>
#include <QPushButton>
#include <QLabel>

namespace gestureLogin {
class WayPointWidget;

class GesturePannel : public QWidget
{
    Q_OBJECT

public:
    explicit GesturePannel(QWidget *parent = nullptr);

    void init();
    void initConnect();

    inline bool getStarted() const { return m_pathStarted; }
    void reset();

Q_SIGNALS:
    void pathStarted(const QPoint &);
    void pathStopped();
    void gestureAuthFaile();
    void gestureAuthDone();

    void inputError();
    void inputDone();

    void authError();
    void authDone();

protected:
    virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    virtual void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    void initPannel();
    void initConnections();

private:
    bool m_pathStarted;
    QPainterPath m_gusturePath;
    QPoint m_currentPos;
    QMap<int, QRectF> m_itemRects;

    QStateMachine *m_stateMachine;
    QMap<WayPointWidget *, QRect> m_widRects;
};
} // namespace gestureLogin
#endif // GESTUREPANNEL_H
