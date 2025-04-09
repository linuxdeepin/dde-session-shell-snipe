// SPDX-FileCopyrightText: 2015 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSCONSTANT_H
#define DBUSCONSTANT_H

#include <QString>

namespace DSS_DBUS {

#ifndef ENABLE_DSS_SNIPE
    const QString accountsService = "com.deepin.daemon.Accounts";
    const QString accountsPath = "/com/deepin/daemon/Accounts";
    const QString accountsUserPath = "/com/deepin/daemon/Accounts/User%1";
    const QString accountsUserInterface = "com.deepin.daemon.Accounts.User";
    const QString loginedPath = "/com/deepin/daemon/Logined";
    const QString powerManagerService = "com.deepin.daemon.PowerManager";
    const QString powerManagerPath = "/com/deepin/daemon/PowerManager";
    const QString powerService = "com.deepin.system.Power";
    const QString powerPath = "/com/deepin/system/Power";
    const QString sessionPowerService = "com.deepin.daemon.Power";
    const QString sessionPowerPath = "/com/deepin/daemon/Power";
    const QString lastoreService = "com.deepin.lastore";
    const QString lastorePath = "/com/deepin/lastore";
    const QString lastoreInterface = "com.deepin.lastore.Manager";
    const QString lockFrontService = "com.deepin.dde.lockFront";
    const QString lockFrontPath = "/com/deepin/dde/lockFront";
    const QString shutdownService = "com.deepin.dde.shutdownFront";
    const QString shutdownPath = "/com/deepin/dde/shutdownFront";
    const QString systemDisplayService = "com.deepin.system.Display";
    const QString systemDisplayPath = "/com/deepin/system/Display";
    const QString inhibitHintService = "com.deepin.InhibitHint";
    const QString inhibitHintPath = "/com/deepin/InhibitHint";
    const QString imageEffectService = "com.deepin.daemon.ImageEffect";
    const QString imageEffectPath = "/com/deepin/daemon/ImageEffect";
    const QString lockService = "com.deepin.dde.LockService";
    const QString lockServicePath = "/com/deepin/dde/LockService";
    const QString zoneService = "com.deepin.daemon.Zone";
    const QString zonePath = "/com/deepin/daemon/Zone";
    const QString sessionManagerService = "com.deepin.SessionManager";
    const QString sessionManagerPath = "/com/deepin/SessionManager";
    const QString authenticateService = "com.deepin.daemon.Authenticate";
    const QString authenticatePath = "/com/deepin/daemon/Authenticate";
    const QString fingerprintAuthPath = "/com/deepin/daemon/Authenticate/Fingerprint";
    const QString fingerprintAuthInterface = "com.deepin.daemon.Authenticate.Fingerprint";
    const QString udcpIamService = "com.deepin.udcp.iam";
    const QString udcpIamPath = "/com/deepin/udcp/iam";
    const QString controlCenterService = "com.deepin.dde.ControlCenter";
    const QString controlCenterPath = "/com/deepin/daemon/ControlCenter";
    const QString systemDaemonService = "com.deepin.daemon.Daemon";
    const QString systemDaemonPath = "/com/deepin/daemon/Daemon";
    const QString systemInfoService = "com.deepin.daemon.SystemInfo";
    const QString systemInfoPath = "/com/deepin/daemon/SystemInfo";
    const QString keybindingService = "com.deepin.daemon.Keybinding";
    const QString keybindingPath = "/com/deepin/daemon/Keybinding";
    const QString screenSaveService = "com.deepin.daemon.ScreenSaver";

#else
    const QString accountsService = "org.deepin.dde.Accounts1";
    const QString accountsPath = "/org/deepin/dde/Accounts1";
    const QString accountsUserPath = "/org/deepin/dde/Accounts1/User%1";
    const QString accountsUserInterface = "org.deepin.dde.Accounts1.User";
    const QString loginedPath = "/org/deepin/dde/Logined";
    const QString powerManagerService = "org.deepin.dde.PowerManager1";
    const QString powerManagerPath = "/org/deepin/dde/PowerManager1";
    const QString powerService = "org.deepin.dde.Power1";
    const QString powerPath = "/org/deepin/dde/Power1";
    const QString sessionPowerService = "org.deepin.dde.Power1";
    const QString sessionPowerPath = "/org/deepin/dde/Power1";
    const QString lastoreService = "org.deepin.dde.Lastore1";
    const QString lastorePath = "/org/deepin/dde/Lastore1";
    const QString lastoreInterface = "org.deepin.dde.Lastore1.Manager";
    const QString lockFrontService = "org.deepin.dde.LockFront1";
    const QString lockFrontPath = "/org/deepin/dde/LockFront1";
    const QString shutdownService = "org.deepin.dde.ShutdownFront1";
    const QString shutdownPath = "/org/deepin/dde/ShutdownFront1";
    const QString systemDisplayService = "org.deepin.dde.Display1";
    const QString systemDisplayPath = "/org/deepin/dde/Display1";
    const QString inhibitHintService = "org.deepin.dde.InhibitHint1";
    const QString inhibitHintPath = "/org/deepin/dde/InhibitHint1";
    const QString imageEffectService = "org.deepin.dde.ImageEffect1";
    const QString imageEffectPath = "/org/deepin/dde/ImageEffect1";
    const QString lockService = "org.deepin.dde.LockService1";
    const QString lockServicePath = "/org/deepin/dde/LockService1";
    const QString zoneService = "org.deepin.dde.Zone1";
    const QString zonePath = "/org/deepin/dde/Zone1";
    const QString sessionManagerService = "org.deepin.dde.SessionManager1";
    const QString sessionManagerPath = "/org/deepin/dde/SessionManager1";
    const QString authenticateService = "org.deepin.dde.Authenticate1";
    const QString authenticatePath = "/org/deepin/dde/Authenticate1";
    const QString fingerprintAuthPath = "/org/deepin/dde/Authenticate1/Fingerprint";
    const QString fingerprintAuthInterface = "org.deepin.dde.Authenticate1.Fingerprint";
    // TODO udcp path
    const QString udcpIamService = "com.deepin.udcp.iam";
    const QString udcpIamPath = "/com/deepin/udcp/iam";
    const QString controlCenterService = "org.deepin.dde.ControlCenter1";
    const QString controlCenterPath = "/org/deepin/dde/ControlCenter1";
    const QString systemDaemonService = "org.deepin.dde.Daemon1";
    const QString systemDaemonPath = "/org/deepin/dde/Daemon1";
    const QString systemInfoService = "org.deepin.dde.SystemInfo1";
    const QString systemInfoPath = "/org/deepin/dde/SystemInfo1";
    const QString keybindingService = "org.deepin.dde.Keybinding1";
    const QString keybindingPath = "/org/deepin/dde/Keybinding1";
    const QString screenSaveService = "org.freedesktop.ScreenSaver";

#endif
}

#endif //DBUSCONSTANT_H
