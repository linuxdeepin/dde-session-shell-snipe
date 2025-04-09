// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbuslockagent.h"
#include "sessionbasemodel.h"
#include "dbusconstant.h"

#include <QDBusMessage>

DBusLockAgent::DBusLockAgent(QObject *parent) : QObject(parent), m_model(nullptr)
{

}

void DBusLockAgent::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

void DBusLockAgent::Show()
{
    getCallerBySender();

    if (isUpdating())
        return;

    m_model->setIsBlackMode(false);
    m_model->setIsHibernateModel(false);
    m_model->setVisible(true);
}

bool DBusLockAgent::Visible() const
{
    return m_model->visible();
}

void DBusLockAgent::ShowAuth(bool active)
{
    getCallerBySender();

    if (isUpdating())
        return;

    Show();
    m_model->activeAuthChanged(active);
}

// 待机，enable=true：进入待机；enable=false：待机恢复
void DBusLockAgent::Suspend(bool enable)
{
    getCallerBySender();

    qCInfo(DDE_SHELL) << (enable ? "Enter suspend" : "Suspend recovery");
    if (isUpdating())
        return;

    if (enable) {
        m_model->setIsBlackMode(true);
        m_model->setVisible(true);
    } else {
        QDBusInterface infc(DSS_DBUS::sessionPowerService, DSS_DBUS::sessionPowerPath,DSS_DBUS::sessionPowerService);
        // 待机恢复需要密码
        bool bSuspendLock = infc.property("SleepLock").toBool();

        if (bSuspendLock) {
            m_model->setIsBlackMode(false);
            m_model->setVisible(true);
        } else {
            m_model->setVisible(false);
            emit m_model->visibleChanged(false);
        }
    }
}

void DBusLockAgent::Hibernate(bool enable)
{
    getCallerBySender();

    qCInfo(DDE_SHELL) << (enable ? "Enter hibernate" : "Hibernate recovery");
    if (isUpdating())
        return;

    m_model->setIsHibernateModel(enable);
    m_model->setVisible(true);
}

void DBusLockAgent::ShowUserList()
{
    getCallerBySender();

    if (isUpdating())
        return;

    emit m_model->showUserList();
}

/**
 * @brief 如果处于更新状态,那么不响应shutdown接口
 */
bool DBusLockAgent::isUpdating() const
{
    const bool updating = m_model->currentContentType() == SessionBaseModel::UpdateContent;
    return updating;
}

void DBusLockAgent::getPathByPid(quint32 pid)
{
    if (pid <= 0) {
        return;
    }

    QString cmdlinePath = QString("/proc/%1/cmdline").arg(pid);
    if (QFile::exists(cmdlinePath)) {
        QFile file(cmdlinePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray line = file.readAll().trimmed();
            QString path = QString::fromLocal8Bit(line.split(' ').last());
            qWarning() << "Caller path : " << path;
        }
    }
}

void DBusLockAgent::getPPidByPid(quint32 pid)
{
    if (pid <= 0) {
        return;
    }

    QString statusFilePath = QString("/proc/%1/status").arg(pid);
    if (!QFile::exists(statusFilePath)) {
        qWarning() << " File not exists :" <<statusFilePath;
        return;
    }

    QFile file(statusFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << " Read status file failed";
        return;
    }

    QByteArray byteArray = file.readAll();
    file.close();

    QTextStream textStream(&byteArray);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    textStream.setEncoding(QStringConverter::Utf8);
#else
    textStream.setCodec("UTF-8");
#endif
    while (!textStream.atEnd()) {
        QString line = textStream.readLine();
        if (line.startsWith("PPid:")) {
            QStringList parts = line.split(QChar(':'));
            if (parts.size() > 1) {
                bool ok = false;
                 quint32 ppid = parts.at(1).trimmed().toInt(&ok);
                 if (ok) {
                     getPathByPid(ppid);
                     getPPidByPid(ppid);
                 }
            }
        }
    }
}

void DBusLockAgent::getCallerBySender()
{
    QDBusMessage msg = message();

    if (msg.type() == QDBusMessage::MethodCallMessage) {
        QString caller = msg.service();
        if (!caller.isEmpty()) {
            QDBusInterface dbusInterface("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", QDBusConnection::sessionBus());
            // 调用 GetConnectionUnixProcessID 方法查询 PID
            QDBusReply<quint32> reply = dbusInterface.call("GetConnectionUnixProcessID", caller);
            if (reply.isValid()) {
                quint32 pid = reply.value();
                qWarning() << "Caller Pid :" << pid;

                getPathByPid(pid);
                getPPidByPid(pid);
           } else {
               qWarning() << "Get caller pid failed :" << reply.error().message();
           }
       }
   }
}
