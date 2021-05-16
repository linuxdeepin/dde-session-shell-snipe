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

#ifndef WIRELESSPAGE_H
#define WIRELESSPAGE_H

#include "src/global_util/wireless/wirelessdevice.h"
#include <dloadingindicator.h>

#include <DSwitchButton>
#include <DStyleOption>
#include <DStyleHelper>
#include <DListView>
#include <DSpinner>
#include <QPointer>
#include <QScroller>
#include <QLabel>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

QT_BEGIN_NAMESPACE
class QTimer;
class QListView;
class QVBoxLayout;
class QStandardItemModel;
class WirelessEditWidget;
QT_END_NAMESPACE

namespace dtk {

namespace wireless {

class NetworkWorker;
class WirelessDevice;

enum WiFiStrenthLevel {
    WiFiStrengthNoNE,
    WiFiStrengthNoLevel,
    WiFiStrengthLOWLevel,
    WiFiStrengthMidLevel,
    WiFiStrengthMidHighLevel,
    WiFiStrengthHighLevel,
};

struct APSortInfo {
    int signalstrength;
    QString ssid;
    bool connected;
    bool operator <(const APSortInfo &other)
    {
        if (connected ^ other.connected) {
            return !connected;
        }
        if (signalstrength != other.signalstrength) {
            return signalstrength < other.signalstrength;
        }
        return ssid < other.ssid;
    }
};

class APItem : public DStandardItem
{
public:
    explicit APItem(QStyle *style, DTK_WIDGET_NAMESPACE::DListView *parent = nullptr);
    virtual ~APItem() override;

    void setSignalStrength(int ss);
    int signalStrength() const;
    bool isConnected();
    void setSortInfo(const APSortInfo &si);
    APSortInfo sortInfo();
    bool operator<(const QStandardItem &other) const override;
public:
    enum {
        SortRole = Qt::UserRole + 1,
        SSIDRole,
        SecureRole
    };
private:
    DListView *m_parentView;
    DViewItemAction *m_secureAction;
    DTK_WIDGET_NAMESPACE::DStyleHelper m_dStyleHelper;
    bool m_preLoading;
    QString m_uuid;
    QPointer<DViewItemAction> m_loadingAction;
    QPointer<DTK_WIDGET_NAMESPACE::DSpinner> m_loadingIndicator;
};

class WirelessPage : public QWidget
{
    Q_OBJECT

public:
    enum WifiStatus {
        Wifi_Unknown = 0,
        Wifi_Available,
        Wifi_Unavailable
    };
    Q_ENUM(WifiStatus)

public:
    explicit WirelessPage(const QString locale, WirelessDevice *device, QWidget *parent = nullptr);
    static WirelessPage *getInstance(const QString locale, WirelessDevice *device, QWidget *parent = nullptr);
    ~WirelessPage() override;

    void setWorker(dtk::wireless::NetworkWorker *worker);
    inline bool isHideNetwork() { return m_isHideNetwork;}
    void updateWiFiStrengthDisplay();
    WiFiStrenthLevel getWiFiSignalStrengthLevel(int ss);
    void connectWirelessErrorHandle(const Device::StateChangeReason &reason);
    void updateWirelessListViewDisplay(bool checked);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateActiveAp();

Q_SIGNALS:
    void requestConnectAp(const QString &apPath, const QString &uuid) const;
    void requestDeviceAPList(const QString &devPath) const;
    void requestWirelessScan();
    void requestDeviceEnabled(const QString &devPath, const bool enabled) const;
    void requestShowAPEditPage(dtk::wireless::WirelessDevice *device, const QString &session) const;
    void requestRemoveAPEditPage(dtk::wireless::WirelessDevice *device) const;
    void requestRefresh() const;
    void requestRefreshWiFiStrengthDisplay(int wifiStrength);

public Q_SLOTS:
    void onAPAdded(const QString &path);
    void onAPChanged(const QString &path);
    void onAPRemoved(const QString &apPath);
    void onActiveAPChanged();
    void onDeviceStatusChanged(NetworkManager::Device::State newstate, NetworkManager::Device::State oldstate, NetworkManager::Device::StateChangeReason reason);

private Q_SLOTS:
    void sortAPList();
    void onNetworkAdapterChanged(bool checked);
    void updateLayout(bool enabled);
    void refreshNetwork();

private:
    bool m_isHideNetwork;
    int m_layoutCount;
    WifiStatus m_preWifiStatus;
    QString m_autoConnectHideSsid;
    QString m_localeName;
    QTimer *m_sortDelayTimer;
    DLoadingIndicator *m_loadingIndicator;
    Dtk::Widget::DSwitchButton *m_switchBtn;
    DListView *m_lvAP;
    QStandardItemModel *m_modelAP;
    APItem *m_titleItem;
    WirelessDevice *m_device;
    dtk::wireless::NetworkWorker *m_worker;
    WirelessEditWidget *m_clickedItemWidget;
    WirelessEditWidget *m_activingItemWidget;
    WirelessEditWidget *m_connectItemWidget;
    AccessPoint::Ptr m_activeAp;
    QVBoxLayout *m_mainLayout;
    QMap<QString, QString> m_ApList;
    QMap<QString, APItem *> m_apItems;
    QMap<QString, WirelessEditWidget *> m_apItemsWidget;
    QLabel *m_lblTitle;
};

} //namespace wireless

} //namespace dtk

Q_DECLARE_METATYPE(dtk::wireless::APSortInfo)
#endif // WIRELESSPAGE_H
