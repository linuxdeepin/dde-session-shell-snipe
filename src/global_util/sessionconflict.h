// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONCONFLICT_H
#define SESSIONCONFLICT_H

#include "dbuslogin1manager.h"

#include <QString>

namespace SessionConflict {
constexpr int kMaxRemoteLoginUsers = 2;
bool isCurrentSessionRemote(DBusLogin1Manager *login1Inter);
bool hasActiveSessionConflict(DBusLogin1Manager *login1Inter, const QString &username);
int countActiveRemoteUsers(DBusLogin1Manager *login1Inter, const QString &excludeUsername);
}

#endif // SESSIONCONFLICT_H
