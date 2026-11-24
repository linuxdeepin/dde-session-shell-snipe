// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionconflict.h"

#include "constants.h"
#include "dbusvariant.h"

#include <QDBusInterface>
#include <QSet>

namespace {

bool isGraphicalSessionType(const QString &type)
{
    return type == QLatin1String("x11") || type == QLatin1String("wayland") || type == QLatin1String("mir");
}

// 真实远程用户会话由 lightdm 启动，service 为 "lightdm"；
// greeter 登录界面的 service 是 "lightdm-greeter"，不计入远程用户。
bool isLightdmService(const QString &service)
{
    return service == QLatin1String("lightdm");
}

QVariant sessionProperty(const QString &sessionPath, const QString &name)
{
    QDBusInterface props("org.freedesktop.login1", sessionPath, "org.freedesktop.DBus.Properties",
                         QDBusConnection::systemBus());
    QDBusReply<QVariant> reply = props.call("Get", "org.freedesktop.login1.Session", name);
    if (!reply.isValid()) {
        return QVariant();
    }
    return reply.value();
}

} // namespace

bool SessionConflict::isCurrentSessionRemote(DBusLogin1Manager *login1Inter)
{
    static QPair<bool, bool> cached {false, false}; // <query succeeded, is remote>
    if (cached.first) {
        return cached.second;
    }

    if (login1Inter == nullptr || !login1Inter->isValid()) {
        qCWarning(DDE_SHELL) << "Login1 not available, treat current session as local";
        return false;
    }

    QDBusPendingReply<QDBusObjectPath> sessionReply = login1Inter->GetSessionByPID(0);
    sessionReply.waitForFinished();
    if (!sessionReply.isValid()) {
        return false;
    }

    const QVariant remote = sessionProperty(sessionReply.value().path(), QStringLiteral("Remote"));
    if (!remote.isValid()) {
        qCWarning(DDE_SHELL) << "Failed to read current session Remote property";
        return false;
    }

    cached.first = true;
    cached.second = remote.toBool();
    return cached.second;
}

bool SessionConflict::hasActiveSessionConflict(DBusLogin1Manager *login1Inter, const QString &username)
{
    if (username.isEmpty() || username == "..." || login1Inter == nullptr || !login1Inter->isValid()) {
        qCWarning(DDE_SHELL) << "Login1 not available, skip active session check";
        return false;
    }

    const bool currentRemote = isCurrentSessionRemote(login1Inter);
    qCInfo(DDE_SHELL) << "Checking active session conflicts, current remote:" << currentRemote;
    QDBusPendingReply<SessionList> reply = login1Inter->ListSessions();
    reply.waitForFinished();
    if (!reply.isValid()) {
        qCWarning(DDE_SHELL) << "ListSessions failed:" << reply.error().message();
        return false;
    }

    const SessionList sessions = reply.value();
    for (const SessionInfo &info : sessions) {
        if (info.user != username) {
            continue;
        }

        QDBusPendingReply<QDBusObjectPath> sessionReply = login1Inter->GetSession(info.session);
        sessionReply.waitForFinished();
        if (!sessionReply.isValid()) {
            continue;
        }

        const QString sessionPath = sessionReply.value().path();
        const QVariant type = sessionProperty(sessionPath, QStringLiteral("Type"));
        const QVariant remote = sessionProperty(sessionPath, QStringLiteral("Remote"));
        if (!type.isValid() || !remote.isValid()) {
            continue;
        }

        const QString typeValue = type.toString();
        if (!isGraphicalSessionType(typeValue)) {
            continue;
        }

        const bool remoteValue = remote.toBool();
        qCInfo(DDE_SHELL) << "Found session for user" << username << ", type:" << typeValue << ", remote:" << remoteValue;
        if ((currentRemote && !remoteValue) || (!currentRemote && remoteValue)) {
            return true;
        }
    }

    return false;
}

int SessionConflict::countActiveRemoteUsers(DBusLogin1Manager *login1Inter, const QString &excludeUsername)
{
    if (login1Inter == nullptr || !login1Inter->isValid()) {
        qCWarning(DDE_SHELL) << "Login1 not available, skip remote user count";
        return 0;
    }

    QDBusPendingReply<SessionList> reply = login1Inter->ListSessions();
    reply.waitForFinished();
    if (!reply.isValid()) {
        qCWarning(DDE_SHELL) << "ListSessions failed:" << reply.error().message();
        return 0;
    }

    QSet<QString> remoteUsers;
    const SessionList sessions = reply.value();
    for (const SessionInfo &info : sessions) {
        if (info.user.isEmpty() || info.user == excludeUsername) {
            continue;
        }

        QDBusPendingReply<QDBusObjectPath> sessionReply = login1Inter->GetSession(info.session);
        sessionReply.waitForFinished();
        if (!sessionReply.isValid()) {
            continue;
        }

        const QString sessionPath = sessionReply.value().path();
        const QVariant remote = sessionProperty(sessionPath, QStringLiteral("Remote"));
        const QVariant service = sessionProperty(sessionPath, QStringLiteral("Service"));
        if (!remote.isValid() || !service.isValid()) {
            continue;
        }

        if (!remote.toBool()) {
            continue;
        }
        if (!isLightdmService(service.toString())) {
            continue;
        }

        remoteUsers.insert(info.user);
    }

    qCInfo(DDE_SHELL) << "Active remote users count:" << remoteUsers.size()
            << ", excluding:" << excludeUsername;
    return remoteUsers.size();
}
