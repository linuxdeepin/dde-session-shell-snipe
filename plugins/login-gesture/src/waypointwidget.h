// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WAYPOINTWIDGET_H
#define WAYPOINTWIDGET_H

#include <QWidget>

namespace gestureLogin {

class WayPointWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WayPointWidget(QWidget *parent = nullptr);
    static QList<int> getCrossedNode(int startId, int stopId);

    void setId(int);
    inline int id() { return m_id; }

public Q_SLOTS:
    void onSelectedById(int m_id, bool arrived); // 选中、撤消图形变化
    void onPathStarted(const QPoint &);
    void onPathStopped();

Q_SIGNALS:
    void arrived(int m_id); // 通知模型处理，如果可选中，再调用onSelectedById

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    QString m_iconPath;
    int m_id;
    bool m_isSelected;
    bool m_started;
};
} // namespace gestureLogin

#endif // WAYPOINTWIDGET_H
