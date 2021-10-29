/*
  * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
  *
  * Author:     dengbo <dengbo@uniontech.com>
  *
  * Maintainer: dengbo <dengbo@uniontech.com>
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */

#ifndef NETWORKWORKER_H
#define NETWORKWORKER_H

#include "wirelessdevice.h"

#include <QMap>
#include <QTimer>
#include <QDBusObjectPath>
#include <QtDBus/QtDBus>

#include <networkmanagerqt/activeconnection.h>

#include <com_deepin_system_systemnetwork.h>

namespace dtk {

namespace wireless {

using SystemNetworkInter = com::deepin::system::Network;

class NetworkWorker : public QObject
{
    Q_OBJECT

public:
    explicit NetworkWorker(QObject *parent = nullptr);
    ~NetworkWorker();

    inline const QList<WirelessDevice *> devices() const { return m_devices; }
    inline const NetworkManager::Connection::List connects() const { return m_connects; }
    inline const NetworkManager::ActiveConnection::List actvieConnects() const { return m_activeConnects; }

private:
    void initWirelessDevice();

Q_SIGNALS:
    void deviceChaged(WirelessDevice *dev,bool isNewDev = true);
    void requestDeviceStatus(const QString &devPath) const;
    void deviceEnableChanged(const QString &device, const bool enabled) const;

public Q_SLOTS:
    void activateConnection(const QString &devPath, const QString &uuid);
    void activateAccessPoint(const QString &apPath, const QString &uuid);
    void requestWirelessScan();
    void queryDeviceStatus(const QString &devPath);
    Q_DECL_DEPRECATED void queryAccessPoints(const QString &devPath);
    void setDeviceEnable(const QString &devPath, const bool enable);

private Q_SLOTS:
    void onDeviceEnableChanged(const QDBusObjectPath &path, const bool enabled);
    void queryDeviceStatusCB(QDBusPendingCallWatcher *w);
    void onDeviceAdd(const QString &uni);
    void onDeviceRemove(const QString &uni);
    void updateConnects();
    void updateActiveConnects(const QString &activeConnPath);

private:
    SystemNetworkInter m_systemNetworkInter;
    QList<dtk::wireless::WirelessDevice *> m_devices;
    NetworkManager::Connection::List m_connects;
    NetworkManager::ActiveConnection::List m_activeConnects;
};

}   // namespace wireless

}   // namespace dtk

#endif // NETWORKWORKER_H
