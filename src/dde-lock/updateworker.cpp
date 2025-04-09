// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updateworker.h"
#include "constants.h"
#include "dbusconstant.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include <DDBusSender>

UpdateWorker::UpdateWorker(QObject* parent)
    : QObject(parent)
{

}

void UpdateWorker::doUpdate(bool powerOff)
{
    qCInfo(DDE_SHELL) << "Start update progress";

    // 检查电量
    const bool powerIsOK = checkPower();
    if (!powerIsOK) {
        Q_EMIT requestExitUpdating();
        DDBusSender()
            .service("org.freedesktop.Notifications")
            .path("/org/freedesktop/Notifications")
            .interface("org.freedesktop.Notifications")
            .method(QString("Notify"))
            .arg(tr("Update"))
            .arg(static_cast<uint>(0))
            .arg(QString("package-updated-failed"))
            .arg(QString(""))
            .arg(tr("Please plug in and then install updates."))
            .arg(QStringList())
            .arg(QVariantMap())
            .arg(5000)
            .call();

        qCWarning(DDE_SHELL) << "Low power, exit update progress";
        return;
    }

    qCInfo(DDE_SHELL) << "Power check passed";

    prepareFullScreenUpgrade(powerOff);
}

bool UpdateWorker::checkPower()
{
    qCInfo(DDE_SHELL) << "Check power";
    PowerInter powerInter(DSS_DBUS::powerService, DSS_DBUS::powerPath, QDBusConnection::systemBus(), this);
    bool onBattery = powerInter.onBattery();
    if (!onBattery) {
        qCInfo(DDE_SHELL) << "No battery";
        return true;
    }

    double data = powerInter.batteryPercentage();
    int batteryPercentage = uint(qMin(100.0, qMax(0.0, data)));
    qCInfo(DDE_SHELL) << "Battery percentage: " << batteryPercentage;
    return batteryPercentage >= 60;
}

void UpdateWorker::prepareFullScreenUpgrade(bool powerOff)
{
    qCInfo(DDE_SHELL) << "Do update after restart lightdm";

    QJsonObject content;
    QDBusInterface managerInter(DSS_DBUS::lastoreService,
        DSS_DBUS::lastorePath,
        DSS_DBUS::lastoreInterface,
        QDBusConnection::systemBus());
    content["DoUpgradeMode"] = managerInter.property("CheckUpdateMode").toInt();
    content["IsPowerOff"] = powerOff;

    QJsonDocument jsonDoc;
    jsonDoc.setObject(content);

    const QString& arg = jsonDoc.toJson();
    qCInfo(DDE_SHELL) << "Call function `PrepareFullScreenUpgrade` with arg:" << arg;

    QDBusPendingReply<QDBusObjectPath> reply = managerInter.asyncCall("PrepareFullScreenUpgrade", arg);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [reply, watcher] {
        watcher->deleteLater();
        if (reply.isValid()) {
            qCInfo(DDE_SHELL) << "Call `PrepareFullScreenUpgrade` function successfully, quit myself";
        } else {
            qCWarning(DDE_SHELL) << "Call `PrepareFullScreenUpgrade` function failed, error:" << reply.error().message();
        }
    });
}
