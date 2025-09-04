// SPDX-FileCopyrightText: 2018 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gesturedialog.h"

#include <DApplication>
#include <DLog>

#include <QCommandLineOption>
#include <QDBusReply>
#include <QDBusInterface>

#include <csignal>

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

using namespace gestureLogin;
using namespace gestureSetting;

const bool IsWayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");

double getScaleFormConfig()
{
    double defaultScaleFactor = 1.0;
    QDBusInterface configInter("com.deepin.system.Display",
                                                     "/com/deepin/system/Display",
                                                     "com.deepin.system.Display",
                                                    QDBusConnection::systemBus());
    if (!configInter.isValid()) {
        return defaultScaleFactor;
    }
    QDBusReply<QString> configReply = configInter.call("GetConfig");
    if (configReply.isValid()) {
        QString config = configReply.value();
        QJsonParseError jsonError;
        QJsonDocument jsonDoc(QJsonDocument::fromJson(config.toLatin1(), &jsonError));
        if (jsonError.error == QJsonParseError::NoError) {
            QJsonObject rootObj = jsonDoc.object();
            QJsonObject Config = rootObj.value("Config").toObject();
            double scaleFactor = Config.value("ScaleFactors").toObject().value("ALL").toDouble();
            qDebug() << "Scale factor from system display config: " << scaleFactor;
            if(scaleFactor == 0.0) {
                scaleFactor = defaultScaleFactor;
            }
            return scaleFactor;
        }
        return defaultScaleFactor;
    }

    qWarning() << "DBus call `GetConfig` failed, reply is invaild, error: " << configReply.error().message();
    return defaultScaleFactor;
}

void setQtScaleFactorEnv()
{
    const double scaleFactor = getScaleFormConfig();
    qDebug() << "Final scale factor: " << scaleFactor;
    if (scaleFactor > 0.0) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scaleFactor).toStdString().c_str());
    } else {
        qputenv("QT_SCALE_FACTOR", "1");
    }
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("XDG_CURRENT_DESKTOP")) {
        qputenv("XDG_CURRENT_DESKTOP", "Deepin");
    }

    if (IsWayland) {
        setQtScaleFactorEnv();
    }

    QString user(qgetenv("USER"));
    if (!user.isEmpty()) {
        QString runtimeDirStr = QString("/tmp/runtime-%1").arg(user);
        QFileInfo runtimeDir(runtimeDirStr);
        if (runtimeDir.exists()) {
            if (runtimeDir.isDir() && runtimeDir.owner() == user) {
                qputenv("XDG_RUNTIME_DIR", runtimeDirStr.toUtf8());
                qputenv("DBUS_SESSION_BUS_ADDRESS", QString("%1/bus").arg(runtimeDirStr).toUtf8());
            }
        } else {
            qputenv("XDG_RUNTIME_DIR", runtimeDirStr.toUtf8());
            qputenv("DBUS_SESSION_BUS_ADDRESS", QString("%1/bus").arg(runtimeDirStr).toUtf8());
        }
    }

    DApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(true);
    a.setAttribute(Qt::AA_EnableHighDpiScaling, true);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    a.setOrganizationName("deepin");
    a.setApplicationName("reset-pattern-dialog");

    // 由于这个应用是由deepin-password-admin管理，因此需要指定日志目录
    QString logPath = QString("/tmp/%1/%1.log").arg(a.applicationName());
    QDir logDir = QFileInfo(logPath).dir();
    if (!logDir.exists())
        logDir.mkdir(logDir.path());
    DLogManager::setlogFilePath(logPath);
    DLogManager::registerFileAppender();
    DLogManager::registerConsoleAppender();

    QCommandLineOption oldPwdOption(QStringList() << "old", "read current password which set it", "oldPasswordId");
    QCommandLineOption newPwdOption(QStringList() << "new", "write the new password which you will set", "newPasswordId");
    QCommandLineOption userOption(QStringList() << "u", "caller's name", "user");
    QCommandLineOption fOption(QStringList() << "f", "caller's full name", "f");
    QCommandLineOption aOption(QStringList() << "a", "call's app", "a");
    QCommandLineParser parser;
    parser.addOption(oldPwdOption);
    parser.addOption(newPwdOption);
    parser.addOption(userOption);
    parser.addOption(fOption);
    parser.addOption(aOption);
    parser.process(a);


    int old_file_id = -1;
    int new_file_id = -1;
    if (parser.isSet(oldPwdOption)) {
        old_file_id = parser.value(oldPwdOption).toInt();
    }
    if (parser.isSet(newPwdOption)) {
        new_file_id = parser.value(newPwdOption).toInt();
    }
    Manager manager(old_file_id, new_file_id);
    manager.start();

    std::signal(SIGTERM, Manager::exit);
    std::signal(SIGKILL, Manager::exit);

    return a.exec();
}
