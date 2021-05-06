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

#include "wirelessdevice.h"

using namespace dtk::wireless;

dtk::wireless::WirelessDevice::WirelessDevice(const QString &path, QObject *parent)
    : NetworkManager::WirelessDevice(path, parent)
{

}

/**
 * @brief 获取当前的所有的热点
 *
 * @param void
 * @return QStringList 对应的热点列表
 */
const QStringList dtk::wireless::WirelessDevice::apList() const
{
    auto devList = NetworkManager::networkInterfaces();

    QStringList apList;

    for (auto device : devList) {
        if (device->type() == NetworkManager::Device::Wifi) {
            WirelessDevice *wirelessDev = new WirelessDevice(device->uni());

            apList.append(wirelessDev->accessPoints());
        }

    }

    return apList;
}

void dtk::wireless::WirelessDevice::setEnabled(const bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        Q_EMIT enableChanged(m_enabled);
    }
}

dtk::wireless::WirelessDevice::~WirelessDevice()
{

}


