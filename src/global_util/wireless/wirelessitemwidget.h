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

#ifndef WIRELESSITEMWIDGET_H
#define WIRELESSITEMWIDGET_H

#include "src/global_util/buttontuple.h"
#include "src/global_util/statebutton.h"
#include <dloadingindicator.h>

#include <QWidget>
#include <QLabel>
#include <QPointer>
#include <DStyleHelper>
#include <DSpinner>
#include <DPasswordEdit>
#include <QStandardItemModel>

#include <networkmanagerqt/manager.h>
#include <networkmanagerqt/accesspoint.h>
#include <networkmanagerqt/connectionsettings.h>
#include <networkmanagerqt/wirelesssetting.h>
#include <networkmanagerqt/security8021xsetting.h>
#include <networkmanagerqt/wirelesssecuritysetting.h>
#include <networkmanagerqt/connection.h>

DWIDGET_USE_NAMESPACE

using namespace NetworkManager;

class WirelessEditWidget : public QWidget
{
    Q_OBJECT
public:
    bool isHiddenNetWork;
    int m_signalStrength;
    QString m_ssid;
    QString m_apPath;
    QString m_connectionUuid;
    QString m_itemName;

    explicit WirelessEditWidget(const QString &ItemName, QWidget *parent = nullptr);
    ~ WirelessEditWidget();
    void setItemWidgetInfo(const AccessPoint *ap);
    void setWidgetVisible(bool enable);
    void setHiddenNetWork(const QString &info);
    void setClickItem(QStandardItem *clickItem);
    void setCurrentAvailableItemWidgets(QMap<QString, WirelessEditWidget *> availableItemWidgets);
    void setClickItemWidget(WirelessEditWidget *clickItemWidget);
    void setCurrentActiveConnect(NetworkManager::ActiveConnection::Ptr activeConnect);
    void updateItemDisplay();
    void setConnectIconVisible(bool enabled);
    void setCurrentConnectItemWidget(WirelessEditWidget *connectItemWidget);
    void updateItemWidgetDisplay(const QString &ssid, const int &signalStrength, const bool isSecurity);
    inline dcc::widgets::ButtonTuple *getButtonTuple() {return m_buttonTuple;}
    void setDevPath(const QString &devPath);
    bool getConnectIconStatus();
    void setSignalStrength(int strength);
    void connectWirelessFailedTips(const Device::StateChangeReason &reason);
    void setWirelessSettings();

private:
    void intiUI(const QString &itemName);
    void initConnection();
    bool passwdInputValid();
    bool ssidInputValid();
    int connectionSuffixNum(const QString &matchConnName);

Q_SIGNALS:
    void confirmPassword(const QString &password);
    void saveSettingsDone();
    void prepareConnectionDone();
    void editingFinished();
    void activateWirelessConnection(const QString &ssid, const QString &uuid);

public Q_SLOTS:
    void updateIndicatorDisplay(bool enable);

private Q_SLOTS:
    void saveConnSettings();
    void prepareConnection();
    void updateConnection();
    void createConnSettings();
    void onBtnClickedHandle();
    void onRequestConnect();

private:
    QString m_devPath;
    QLabel *m_ssidLable;
    QLabel *m_securityLabel;
    QLabel *m_strengthLabel;
    StateButton *m_stateButton;
    DSpinner *m_loadingStat;

    DLineEdit *m_ssidLineEdit;
    DPasswordEdit *m_passwdEdit;
    dcc::widgets::ButtonTuple *m_buttonTuple;

    QPixmap m_securityPixmap;

    QStandardItem *m_clickedItem;
    NetworkManager::ActiveConnection::Ptr m_currentActiveConnect;
    WirelessEditWidget *m_clickedItemWidget;
    WirelessEditWidget *m_connectItemWidget;
    QMap<QString, WirelessEditWidget *> m_availableItemWidgets;
    QWidget *m_wirelessInfoWidget;
    QWidget *m_wirelessEditWidget;

    NetworkManager::Connection::Ptr m_connection;
    NetworkManager::ConnectionSettings::Ptr m_connectionSettings;
    NetworkManager::WirelessSetting::Ptr m_wirelessSetting;
    NetworkManager::WirelessSecuritySetting::Ptr m_wsSetting;
};

#endif // WIRELESSITEMWIDGET_H
