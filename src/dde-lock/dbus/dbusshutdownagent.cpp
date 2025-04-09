// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbusshutdownagent.h"
#include "sessionbasemodel.h"

DBusShutdownAgent::DBusShutdownAgent(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
{

}

void DBusShutdownAgent::setModel(SessionBaseModel * const model)
{
    m_model = model;
}

void DBusShutdownAgent::show()
{
    getCallerBySender();

    if (isUpdating() || !canShowShutDown())
        return;

    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
}

void DBusShutdownAgent::Shutdown()
{
    qCInfo(DDE_SHELL) << "Shutdown";

    getCallerBySender();

    if (isUpdating())
        return;

    if (!canShowShutDown()) {
         //锁屏时允许关机
        emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, false);
    } else {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
        m_model->setVisible(true);
        emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, true);
    }
}

void DBusShutdownAgent::UpdateAndShutdown()
{
    qCInfo(DDE_SHELL) << "Update and shutdown";

    getCallerBySender();

    if (isUpdating())
        return;

    m_model->setUpdatePowerMode(SessionBaseModel::UPM_UpdateAndShutdown);
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
}

void DBusShutdownAgent::Restart()
{
    qCInfo(DDE_SHELL) << "Restart";

    getCallerBySender();

    if (isUpdating())
        return;

    if (!canShowShutDown()) {
        //锁屏时允许重启
        emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireRestart, false);
    } else {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
        m_model->setVisible(true);
        emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireRestart, true);
    }
}

void DBusShutdownAgent::UpdateAndReboot()
{
    qCInfo(DDE_SHELL) << "Update and reboot";

    getCallerBySender();

    if (isUpdating())
        return;

    m_model->setUpdatePowerMode(SessionBaseModel::UPM_UpdateAndReboot);
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
}

void DBusShutdownAgent::Logout()
{
    qCInfo(DDE_SHELL) << "Logout";

    getCallerBySender();

    if (isUpdating() || !canShowShutDown())
        return;

    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
    emit m_model->onRequirePowerAction(SessionBaseModel::PowerAction::RequireLogout, true);
}

void DBusShutdownAgent::Suspend()
{
    qCInfo(DDE_SHELL) << "Suspend";

    getCallerBySender();

    if (isUpdating())
        return;

    if (!canShowShutDown()) {
        // 锁屏时允许待机
        m_model->setPowerAction(SessionBaseModel::RequireSuspend);
    } else {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
#ifndef ENABLE_DSS_SNIPE
        if (m_model->getPowerGSettings("", "sleepLock").toBool()) {
#else
        if (isSleepLock()) {
#endif
            m_model->setVisible(true);
        }
        emit m_model->onRequirePowerAction(SessionBaseModel::RequireSuspend, true);
    }
}

void DBusShutdownAgent::Hibernate()
{
    qCInfo(DDE_SHELL) << "Hibernate";

    getCallerBySender();

    if (isUpdating())
        return;

    if (!canShowShutDown()) {
        // 锁屏时允许休眠
        m_model->setPowerAction(SessionBaseModel::RequireHibernate);
    } else {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
#ifndef ENABLE_DSS_SNIPE
        if (m_model->getPowerGSettings("", "sleepLock").toBool()) {
#else
        if (isSleepLock()) {
#endif
            m_model->setVisible(true);
        }
        emit m_model->onRequirePowerAction(SessionBaseModel::RequireHibernate, true);
    }
}

void DBusShutdownAgent::SwitchUser()
{
    qCInfo(DDE_SHELL) << "Switch user";

    getCallerBySender();

    if (isUpdating() || !canShowShutDown())
        return;

    emit m_model->showUserList();
}

void DBusShutdownAgent::Lock()
{
    qCInfo(DDE_SHELL) << "Lock";

    getCallerBySender();

    if (isUpdating() || !canShowShutDown())
        return;

    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
    m_model->setPowerAction(SessionBaseModel::RequireLock);
}

bool DBusShutdownAgent::canShowShutDown() const
{
    // 如果当前界面已显示，而且不是关机模式，则当前已锁屏，因此不允许调用,以免在锁屏时被远程调用而进入桌面
    qCInfo(DDE_SHELL) << "Can show shutdown, visible: " << m_model->visible() << ", mode: " << m_model->currentModeState();

    return !(m_model->visible() && m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode);
}

/**
 * @brief 如果处于更新状态,那么不响应shutdown接口
 */
bool DBusShutdownAgent::isUpdating() const
{
    qCInfo(DDE_SHELL) << "DBus shutdown agent is updating, current content type: " << m_model->currentContentType();
    return m_model->currentContentType() == SessionBaseModel::UpdateContent;
}

void DBusShutdownAgent::getPathByPid(quint32 pid)
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

void DBusShutdownAgent::getPPidByPid(quint32 pid)
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

void DBusShutdownAgent::getCallerBySender()
{
    QString DDE_DEBUG_LEVEL = qgetenv("DDE_DEBUG_LEVEL");
    if ( DDE_DEBUG_LEVEL.toLower() != "debug") {
        return;
    }

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

bool DBusShutdownAgent::Visible() const
{
    return m_model->visible() && (m_model->currentModeState() == SessionBaseModel::ModeStatus::PowerMode || m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode);
}
