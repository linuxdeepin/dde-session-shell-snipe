// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QObject>
#include <QDBusInterface>

namespace gestureLogin {

// 提供da状态查询与监控
class UserService : public QObject
{
    Q_OBJECT
public:
    static UserService *instance();

    void setUserName(const QString &);
    inline bool isServiceValid() { return m_userInter != nullptr; }
    // 查询当前用户手势是否开启
    bool gestureEnabled();
    // 查询当前用户手势状态标识
    int gestureFlags();
    QString localeName();

    void enroll(const QString &);

public Q_SLOTS:
    void onPropertiesChanged(const QDBusMessage& msg);

Q_SIGNALS:
    void userPatternStateChanged(int);

private:
    explicit UserService(const QString &, QObject *parent = nullptr);
    void init();

private:
    QDBusInterface *m_userInter;
    QString m_currentName;
    QString m_userInterpath;
};
} // namespace gestureLogin

#endif // DBUSSERVICE_H
