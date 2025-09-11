// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GESTUREWORKER_H
#define GESTUREWORKER_H

#include <QObject>
#include <QDBusInterface>
#include <QStateMachine>
#include <QTimer>

namespace gestureLogin {
class GestureAuthWorker : public QObject
{
    Q_OBJECT

public:
    GestureAuthWorker(QObject *parent = nullptr);
    void setLocalPassword(const QString &localPassword);

    // 功能已开启
    bool gestureEnabled();

    // 手势已录入
    bool gestureExist();

    void setActive(bool);

private:
    void initConnection();
    void initState();

public Q_SLOTS:
    void onAuthStateChanged(int, int);
    void onLimitsInfoChanged(bool, int, int, int, const QString &);

Q_SIGNALS:
    void requestSendToken(const QString &);

private:
    QDBusInterface *m_userService;
    QStateMachine *m_stateMachine;
    QString m_localPasswd;

    int m_lockMinutes;

    QTimer m_remaindTimer;
    QTimer m_unLockTimer;
};
} // namespace gestureLogin

#endif // GESTUREWORKER_H
