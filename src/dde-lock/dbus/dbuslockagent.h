// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSLOCKAGENT_H
#define DBUSLOCKAGENT_H

#include <QObject>
#include <QDBusContext>

class SessionBaseModel;
class DBusLockAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    explicit DBusLockAgent(QObject *parent = nullptr);
    void setModel(SessionBaseModel *const model);

    void Show();
    void ShowUserList();
    void ShowAuth(bool active);
    void Suspend(bool enable);
    void Hibernate(bool enable);
    bool Visible() const;

private:
    bool isUpdating() const;

    void getPathByPid(quint32 pid);
    void getPPidByPid(quint32 pid);
    void getCallerBySender();

private:
    SessionBaseModel *m_model;
};

#endif // DBUSLOCKAGENT_H
