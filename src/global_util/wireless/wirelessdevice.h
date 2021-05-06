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

#ifndef WIRELESSDEVICE_H
#define WIRELESSDEVICE_H

#include <QStringList>

#include <networkmanagerqt/manager.h>
#include <networkmanagerqt/wirelessdevice.h>

namespace dtk {

namespace wireless {

using namespace NetworkManager;

class WirelessDevice : public NetworkManager::WirelessDevice
{
    Q_OBJECT

public:
    explicit WirelessDevice(const QString &path, QObject *parent = nullptr);
    ~WirelessDevice() override;

    inline  bool enabled() const { return m_enabled; }
    const QStringList apList() const;

Q_SIGNALS:
    void enableChanged(const bool enabled) const;
    void apAdded(const QString &path) const;

public Q_SLOTS:
    void setEnabled(const bool enabled);

private:
    bool m_enabled = false;
};

} // namespace wireless

} //namespace dtk

#endif // WIRELESSDEVICE_H
