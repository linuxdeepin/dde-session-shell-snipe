// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gesturepannel.h"

#include "waypointwidget.h"
#include "waypointmodel.h"

#include <QtMath>
#include <QMouseEvent>
#include <QDebug>
#include <QLabel>
#include <QApplication>
#include <QState>
#include <QPalette>

using namespace gestureLogin;

static int itemsAccount = 9;

GesturePannel::GesturePannel(QWidget *parent)
    : QWidget(parent)
    , m_pathStarted(false)
{
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    setFixedSize(250, 250);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setContentsMargins(0, 0, 0, 0);
    init();
    initConnect();
}

void GesturePannel::init()
{
    if (!itemsAccount) {
        return;
    }

    qreal root = qSqrt(itemsAccount);
    int column = static_cast<int>(root);

    // 只处理N*N的排列图形
    if (itemsAccount % column != 0) {
        return;
    }

    int solidSpaceWidth = this->width() / column;
    int fixWidth = (solidSpaceWidth - 50) / 2;

    m_itemRects.clear();
    for (int index = 0; index < itemsAccount; index++) {
        auto wid = new WayPointWidget(this);
        wid->setFocusPolicy(Qt::NoFocus);
        wid->setId(index);
        // 布局
        int x = this->contentsRect().top() + solidSpaceWidth * (index % column);
        int y = this->contentsRect().left() + solidSpaceWidth * (index / column);
        wid->setFixedSize(52, 52);
        wid->move(x + fixWidth * (index % column), y + fixWidth * (index / column));
        m_widRects.insert(wid, wid->geometry());
        m_itemRects[index] = wid->geometry();

        connect(WayPointModel::instance(), &WayPointModel::selected, wid, &WayPointWidget::onSelectedById);
        connect(wid, &WayPointWidget::arrived, WayPointModel::instance(), &WayPointModel::onPathArrived);
        connect(this, &GesturePannel::pathStarted, wid, &WayPointWidget::onPathStarted);
        connect(this, &GesturePannel::pathStopped, wid, &WayPointWidget::onPathStopped);
    }
}

void GesturePannel::initConnect()
{
    connect(WayPointModel::instance(), &WayPointModel::selected, this, [this](int, bool) {
        update();
    });
    connect(WayPointModel::instance(), &WayPointModel::widgetColorChanged, this, [this] {
        update();
    });

    connect(this, &GesturePannel::pathStopped, WayPointModel::instance(), &WayPointModel::onUserInputDone);

    connect(WayPointModel::instance(), &WayPointModel::inputLocked, this, &QWidget::setDisabled);
    setDisabled(WayPointModel::instance()->locked());
}

void GesturePannel::reset()
{
}

void GesturePannel::mousePressEvent(QMouseEvent *event)
{
    if (!WayPointModel::instance()->locked()) {
        // 起点不一定在结点上，仅记录起始点，交给mouseMove处理连接
        releaseMouse();
        // 当输入小于4个点时，将保留界面直到用户再输入
        WayPointModel::instance()->clearPath();
        m_pathStarted = true;
        Q_EMIT pathStarted(event->pos());

        m_currentPos = event->pos();

        update();
    }

    QWidget::mousePressEvent(event);
}

void GesturePannel::mouseReleaseEvent(QMouseEvent *event)
{
    if (!WayPointModel::instance()->locked()) {
        m_pathStarted = false;
        Q_EMIT pathStopped();

        update();
    }

    QWidget::mouseReleaseEvent(event);
}

// 最后一个结点的连接线
void GesturePannel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pathStarted) {
        m_currentPos = event->pos();
        foreach (auto rect, m_widRects) {
            if (rect.contains(m_currentPos)) {
                auto wid = m_widRects.key(rect);
                Q_EMIT wid->arrived(wid->id());
            }
        }
        update();
    }

    QWidget::mouseMoveEvent(event);
}

//  选中点的样式变化自己去处理，这里只画连接线
void GesturePannel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (WayPointModel::instance()->currentPoints().size()) {
        QVector<QPointF> wayPoints;
        auto currentSelected = WayPointModel::instance()->currentPoints();
        foreach (auto index, currentSelected) {
            wayPoints.push_back(m_itemRects.value(index).center());
        }

        if (m_pathStarted) {
            wayPoints.push_back(m_currentPos);
        }

        auto model = WayPointModel::instance();
        auto colorConfig = model->colorConfig();
        QColor lineColor = model->showErrorStyle() ? colorConfig.warningLine : colorConfig.line;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(lineColor, 2);
        painter.setPen(pen);
        painter.drawPolyline(wayPoints.data(), wayPoints.size());
    }
}
