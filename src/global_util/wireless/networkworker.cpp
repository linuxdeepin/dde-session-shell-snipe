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

#include "networkworker.h"
#include "wirelessdevice.h"

#include <QMetaProperty>
#include <QDBusMessage>

#include <networkmanagerqt/manager.h>
#include <networkmanagerqt/wirelessdevice.h>
#include <networkmanagerqt/accesspoint.h>
#include <networkmanagerqt/settings.h>
#include <networkmanagerqt/device.h>

using namespace dtk::wireless;
using namespace NetworkManager;

NetworkWorker::NetworkWorker(QObject *parent)
    : QObject(parent)
    , m_systemNetworkInter("com.deepin.system.Network", "/com/deepin/system/Network", QDBusConnection::systemBus(), this)
{
    if (m_devices.isEmpty()) {
        initWirelessDevice();
    }

    connect(&m_systemNetworkInter, &SystemNetworkInter::DeviceEnabled, this, &NetworkWorker::onDeviceEnableChanged);

    // 处理设备状态改变
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceAdded, this, &NetworkWorker::onDeviceAdd);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceRemoved, this, &NetworkWorker::onDeviceRemove);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionsChanged, this, &NetworkWorker::updateConnects);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionAdded, this, &NetworkWorker::updateActiveConnects);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionRemoved, this, &NetworkWorker::updateActiveConnects);
}

void NetworkWorker::updateConnects()
{
    const NetworkManager::Connection::List connections = NetworkManager::listConnections();
    m_connects = connections;
}

void NetworkWorker::updateActiveConnects(const QString &activeConnPath)
{
    Q_UNUSED(activeConnPath);
    NetworkManager::ActiveConnection::List activeConns = NetworkManager::activeConnections();

    m_activeConnects = activeConns;
}

void NetworkWorker::setDeviceEnable(const QString &devPath, const bool enable)
{
    m_systemNetworkInter.EnableDevice(devPath, enable);
}

/**
 * @brief 请求WiFi扫描
 *
 * @param void
 * @return void
 */
void NetworkWorker::requestWirelessScan()
{
    for (auto device : NetworkManager::networkInterfaces()) {
        if (device->type() != NetworkManager::Device::Type::Wifi) {
            continue;
        }

        NetworkManager::WirelessDevice::Ptr wDevice = device.staticCast<NetworkManager::WirelessDevice>();

        if (wDevice) {
            wDevice->requestScan();
        }
    }
}

void NetworkWorker::queryDeviceStatus(const QString &devPath)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_systemNetworkInter.IsDeviceEnabled(devPath), this);

    w->setProperty("devPath", devPath);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryDeviceStatusCB);
}

/**
 * @brief 处理设备添加
 *
 * @param uni 改变的对应设备路径
 * @return void
 */
void NetworkWorker::onDeviceAdd(const QString &uni)
{
    QString devPath = uni;
    if (devPath.isEmpty())
        return;

    if (m_devices.isEmpty()) {
        initWirelessDevice();
    }

    // 需要过滤掉其他设备,只有wifi设备才添加
    for (auto device : NetworkManager::networkInterfaces()) {
        if (device->uni() == uni) {
            if (device->type() != NetworkManager::Device::Type::Wifi) {
                return;
            }
        }
    }

    WirelessDevice *newWirelessDevice = new WirelessDevice(uni);
    m_devices.append(newWirelessDevice);
    queryDeviceStatus(newWirelessDevice->uni());

    Q_EMIT deviceChaged(newWirelessDevice);

}

/**
 * @brief 处理设备移除
 *
 * @param uni 改变的对应设备路径
 * @return void
 */
void NetworkWorker::onDeviceRemove(const QString &uni)
{
    for (auto dev : m_devices) {
        if (dev->uni() == uni) {
            m_devices.removeOne(dev);

            Q_EMIT deviceChaged(dev, false);
            break;
        }
    }
}

/**
 * @brief 处理WiFi设备使能
 *
 * @param path 对应设备的路径
 * @return void
 */
void NetworkWorker::onDeviceEnableChanged(const QString &path, const bool enabled)
{
    WirelessDevice *dev = nullptr;
    for (auto const d : m_devices) {
        if (d->uni() == path) {
            dev = d;
            break;
        }
    }

    if (!dev)
        return;

    dev->setEnabled(enabled);

    Q_EMIT deviceEnableChanged(path, enabled);
}

void NetworkWorker::queryDeviceStatusCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<bool> reply = *w;

    onDeviceEnableChanged(w->property("devPath").toString(), reply.value());

    w->deleteLater();
}

void NetworkWorker::queryAccessPoints(const QString &devPath)
{
    Q_UNUSED(devPath)
    return;
}

void NetworkWorker::initWirelessDevice()
{
    //初始化WiFi网络设备
    for (auto device : NetworkManager::networkInterfaces()) {
        if (device->type() != NetworkManager::Device::Type::Wifi) {
            continue;
        }

        WirelessDevice *dev = new WirelessDevice(device->uni());
        if (dev) {
            m_devices.append(dev);
        }

        queryDeviceStatus(dev->uni());
    }
}

/**
 * @brief 激活当前的网络连接
 *
 * @param devPath 对应设备的路径
 * @param uuid 对应设备的标识
 * @return void
 */
void NetworkWorker::activateConnection(const QString &devPath, const QString &uuid)
{
    NetworkManager::Connection::Ptr connection = findConnectionByUuid(uuid);

    if (!connection) {
        NetworkManager::activateConnection(connection->path(), devPath, QString("/"));
    }
}

/**
 * @brief 激活对应的WiFi热点
 *
 * @param apPath 对应的路径
 * @param uuid 对应设备的标识
 * @return void
 */
void NetworkWorker::activateAccessPoint(const QString &apPath, const QString &uuid)
{
    NetworkManager::Connection::Ptr connection = findConnectionByUuid(uuid);
    qDebug() << connection;

    if (connection) {
        NetworkManager::ConnectionSettings::Ptr connectionSettings = connection->settings();

        if (!connectionSettings) {
            NMVariantMapMap connNMVariant = connectionSettings->toMap();
            QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(NetworkManager::addAndActivateConnection(connNMVariant, apPath, QString("/")));

            connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
        }
    }
}

NetworkWorker::~NetworkWorker()
{
    qDeleteAll(m_devices);
    qDebug() << "quit thread";
}

