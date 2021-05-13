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

#include "wirelessitemwidget.h"
#include "src/global_util/imageutil.h"

#include <DGuiApplicationHelper>
#include <DHiDPIHelper>

#include <networkmanagerqt/setting.h>
#include <networkmanagerqt/settings.h>
#include <networkmanagerqt/ipv4setting.h>
#include <networkmanagerqt/ipv6setting.h>
#include <networkmanagerqt/generictypes.h>
#include <networkmanagerqt/utils.h>

#define PLUGIN_ICON_SIZE 20

using namespace dcc::widgets;
using namespace NetworkManager;

WirelessEditWidget::WirelessEditWidget(const QString &ItemName, QWidget *parent)
    : QWidget(parent)
    , isHiddenNetWork(false)
    , isSecurityNetWork(false)
    , m_itemName(ItemName)
{
    if (m_itemName.isEmpty()) {
        return;
    }

    if (m_itemName.contains("hidden")) {
        isHiddenNetWork = true;
    }

    intiUI(m_itemName);

    if (m_connectionUuid.isEmpty()) {
        qDebug() << "connection uuid is empty, creating new ConnectionSettings...";
        createConnSettings();
    } else {
        m_connection = findConnectionByUuid(m_connectionUuid);
        if (!m_connection) {
            qDebug() << "can't find connection by uuid";
            return;
        }
        m_connectionSettings = m_connection->settings();
    }

    initConnection();
}

void WirelessEditWidget::intiUI(const QString &itemName)
{
    // Wifi显示信息布局
    QHBoxLayout *wirelessInfoLayout = new QHBoxLayout;
    wirelessInfoLayout->setContentsMargins(0, 0, 0, 0);
    m_wirelessInfoWidget = new QWidget;
    m_securityLabel = new QLabel();
    m_strengthLabel = new QLabel();
    m_ssidLable = new QLabel(itemName);
    m_stateButton = new StateButton;
    m_ssidLineEdit = new DLineEdit;
    m_ssidLineEdit->setVisible(false);
    m_passwdEdit = new DPasswordEdit;
    m_stateButton->setFixedSize(PLUGIN_ICON_SIZE, PLUGIN_ICON_SIZE);
    m_stateButton->setVisible(false);
    wirelessInfoLayout->addWidget(m_securityLabel);
    wirelessInfoLayout->setContentsMargins(10, 0, 5, 0);
    wirelessInfoLayout->addSpacing(2);
    wirelessInfoLayout->addWidget(m_strengthLabel);
    wirelessInfoLayout->addSpacing(10);
    wirelessInfoLayout->addWidget(m_ssidLable);
    wirelessInfoLayout->addStretch();
    wirelessInfoLayout->addWidget(m_stateButton, 0, Qt::AlignRight);

    m_loadingStat = new DSpinner(this);
    m_loadingStat->setFixedSize(PLUGIN_ICON_SIZE, PLUGIN_ICON_SIZE);
    m_loadingStat->setVisible(false);
    wirelessInfoLayout->addWidget(m_loadingStat, 0, Qt::AlignRight);
    m_wirelessInfoWidget->setLayout(wirelessInfoLayout);

    QFontMetrics fontMetrics(m_ssidLable->font());
    if (fontMetrics.width(m_ssidLable->text()) > m_ssidLable->width()) {
        QString strSsid = QFontMetrics(m_ssidLable->font()).elidedText(m_ssidLable->text(), Qt::ElideRight, m_ssidLable->width());
        m_ssidLable->setText(strSsid);
    }

    // Wifi编辑界面布局
    m_wirelessEditWidget = new QWidget;
    QVBoxLayout *inputEditLayout = new QVBoxLayout;
    inputEditLayout->setContentsMargins(10, 0, 5, 0);

    if (isHiddenNetWork) {
        m_ssidLineEdit->setVisible(true);
        m_ssidLineEdit->lineEdit()->setPlaceholderText(tr("Please input SSID"));
        m_ssidLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    m_passwdEdit->lineEdit()->setPlaceholderText(tr("Please input Wi-Fi password"));
    m_passwdEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_buttonTuple = new ButtonTuple(ButtonTuple::Save);
    QPushButton *cancelBtn = m_buttonTuple->leftButton();
    QPushButton *connectBtn = m_buttonTuple->rightButton();

    cancelBtn->setText(tr("Cancel"));
    QPalette pa = cancelBtn->palette();
    pa.setColor(DPalette::Light, Qt::black);
    pa.setColor(DPalette::Dark, Qt::black);
    pa.setColor(DPalette::ButtonText, Qt::white);
    cancelBtn->setPalette(pa);
    connectBtn->setText(tr("Connect"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(connectBtn);

    if (isHiddenNetWork) {
        inputEditLayout->addWidget(m_ssidLineEdit);
        inputEditLayout->addSpacing(10);
    }

    inputEditLayout->addWidget(m_passwdEdit);

    inputEditLayout->addLayout(btnLayout);
    m_wirelessEditWidget->setLayout(inputEditLayout);

    // 主布局
    QVBoxLayout *mainlayout = new QVBoxLayout;
    mainlayout->setContentsMargins(0, 0, 5, 10);
    mainlayout->addWidget(m_wirelessInfoWidget);
    mainlayout->addSpacing(0);
    mainlayout->addWidget(m_wirelessEditWidget);

    m_wirelessEditWidget->setVisible(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setLayout(mainlayout);
}

WirelessEditWidget::~ WirelessEditWidget()
{
}

void WirelessEditWidget::onRequestConnect()
{
    saveConnSettings();
}

void WirelessEditWidget::updateIndicatorDisplay(bool enable)
{
    if (enable) {
        m_loadingStat->start();
        m_loadingStat->setVisible(true);
        m_loadingStat->show();
    } else {
        m_loadingStat->stop();
        m_loadingStat->hide();
        m_loadingStat->setVisible(false);
    }
}

void WirelessEditWidget::setWidgetVisible(bool enable)
{
    m_wirelessEditWidget->setVisible(enable);
}

void WirelessEditWidget::setConnectIconVisible(bool enabled)
{
    m_stateButton->setVisible(enabled);
}

bool WirelessEditWidget::getConnectIconStatus()
{
    return m_stateButton->isVisible();
}

void WirelessEditWidget::onBtnClickedHandle()
{
    if (m_clickedItemWidget->isHiddenNetWork) {
        m_ssidLineEdit->clear();
    }

    m_passwdEdit->clear();
    m_wirelessEditWidget->setVisible(false);
    m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 50));
}

void WirelessEditWidget::setItemDisplay()
{
    if (m_clickedItemWidget->isHiddenNetWork) {
        m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 180));
    } else if (!m_clickedItemWidget->isSecurityNetWork) {
        setWidgetVisible(false);
        onRequestConnect();
    } else {
        m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 120));
    }
}

void WirelessEditWidget::updateItemDisplay()
{
    if (m_clickedItemWidget->isHiddenNetWork) {
        m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 180));
    } else {
        m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 120));
    }
}

void WirelessEditWidget::setSignalStrength(int strength)
{
    QPixmap iconPix;
    const QSize s = QSize(16, 16);

    QString type;
    if (strength > 65)
        type = "80";
    else if (strength > 55)
        type = "60";
    else if (strength > 30)
        type = "40";
    else if (strength > 5)
        type = "20";
    else
        type = "0";

    QString iconString = QString("wireless-%1-symbolic").arg(type);

    const auto ratio = devicePixelRatioF();
    iconPix = ImageUtil::loadSvg(iconString, ":/img/wireless/", s.width(), ratio);

    m_strengthLabel->setPixmap(iconPix);

    m_securityPixmap = QIcon::fromTheme(":/img/wireless/security.svg").pixmap(s * devicePixelRatioF());
    m_securityPixmap.setDevicePixelRatio(devicePixelRatioF());
    m_securityLabel->setPixmap(m_securityPixmap);
}

void WirelessEditWidget::connectWirelessFailedTips(const Device::StateChangeReason &reason)
{
    Device::StateChangeReason errorReason = reason;

    updateItemDisplay();
    m_wirelessEditWidget->setVisible(true);
    m_passwdEdit->clear();

    if (errorReason == Device::SsidNotFound) {
        if (m_clickedItemWidget->isHiddenNetWork) {
            m_ssidLineEdit->showAlertMessage(tr("The %1 802.11 WLAN network could not be found").arg(m_clickedItemWidget->m_ssid));
        } else {
            m_passwdEdit->showAlertMessage(tr("The %1 802.11 WLAN network could not be found").arg(m_clickedItemWidget->m_ssid));
        }

    } else if (errorReason == Device::NoSecretsReason) {
        m_passwdEdit->showAlertMessage(tr("Connection failed, unable to connect %1, wrong password").arg(m_clickedItemWidget->m_ssid));
    } else if (errorReason == Device::ConfigUnavailableReason) {
        m_passwdEdit->showAlertMessage(tr("Unable to connect %1, please keep closer to the wireless router").arg(m_clickedItemWidget->m_ssid));
    } else if (errorReason == Device::AuthSupplicantTimeoutReason) {
        m_passwdEdit->showAlertMessage(tr("The 802.1X supplicant took too long time to authenticate"));
    }else{
        m_passwdEdit->showAlertMessage(tr("Connect failed"));
    }

    QTimer::singleShot(2000, this, [ = ] {
        if (!m_clickedItemWidget->isSecurityNetWork) {
                setWidgetVisible(false);
                m_clickedItem->setSizeHint(QSize(m_clickedItem->sizeHint().width(), 50));
        }
    });

}

void WirelessEditWidget::setItemWidgetInfo(const AccessPoint *ap)
{
    m_ssid = ap->ssid();
    m_ssidLineEdit->setText(m_ssid);
    m_apPath = ap->uni();
    m_signalStrength = ap->signalStrength();

    setSignalStrength(ap->signalStrength());

    if (!ap->capabilities()) {
        m_securityLabel->clear();
    } else if (!m_securityLabel->pixmap()) {
        m_securityLabel->setPixmap(m_securityPixmap);
    } else {
        isSecurityNetWork = true;
    }
}

void WirelessEditWidget::updateItemWidgetDisplay(const QString &ssid, const int &signalStrength, const bool isSecurity)
{
    m_ssid = ssid;
    m_signalStrength = signalStrength;
    if (m_ssid.isEmpty()) {
        m_wirelessInfoWidget->setVisible(false);
    }

    setSignalStrength(m_signalStrength);

    if (!isSecurity) {
        m_securityLabel->clear();
    } else if (!m_securityLabel->pixmap()) {
        m_securityLabel->setPixmap(m_securityPixmap);
    }

    if (m_ssidLable->text().isEmpty()) {
        m_wirelessInfoWidget->setVisible(false);
    }
}

void WirelessEditWidget::setDevPath(const QString &devPath)
{
    m_devPath = devPath;
}

void WirelessEditWidget::setHiddenNetWork(const QString &info)
{
    m_ssidLable->setText(info);
}

void WirelessEditWidget::initConnection()
{
    connect(m_buttonTuple->rightButton(), &QPushButton::clicked, this, &WirelessEditWidget::onRequestConnect);

    connect(m_buttonTuple->leftButton(), &QPushButton::clicked, this, &WirelessEditWidget::onBtnClickedHandle);
    connect(this, &WirelessEditWidget::saveSettingsDone, this, &WirelessEditWidget::prepareConnection);
    connect(this, &WirelessEditWidget::prepareConnectionDone, this, &WirelessEditWidget::updateConnection);
}

bool WirelessEditWidget::ssidInputValid()
{
    bool valid = true;
    const int length = (m_ssidLineEdit->text().toUtf8().length());
    m_ssid = m_ssidLineEdit->text().toUtf8();

    // 判断输入的SSID的有效性
    if (!(length > 0 && length < 33)) {
        valid = false;
        updateItemDisplay();
        m_ssidLineEdit->showAlertMessage(tr("Invalid SSID"));
        qDebug() << "input ssid is invalid!";
    }

    return valid;
}

bool WirelessEditWidget::passwdInputValid()
{
    bool valid = false;

    valid = NetworkManager::wpaPskIsValid(m_passwdEdit->text());
    // 判断输入的passwd的有效性
    if (!valid) {
        updateItemDisplay();
        m_passwdEdit->showAlertMessage(tr("Invalid password"));
        qDebug() << "input passwd is invalid!";
    }

    return valid;
}

void WirelessEditWidget::setWirelessSettings()
{
    m_wirelessSetting->setSsid(m_ssidLineEdit->text().toUtf8());
    m_wirelessSetting->setHidden(true);
    m_wsSetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaNone);
    m_wirelessSetting->setMode(WirelessSetting::NetworkMode::Infrastructure);

    m_wirelessSetting->setInitialized(true);
    m_wsSetting->setInitialized(false);
    Q_EMIT saveSettingsDone();
}

void WirelessEditWidget::setSecurityWirelessSettings()
{
    m_wsSetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaPsk);
    m_wirelessSetting->setSsid(m_ssidLineEdit->text().toUtf8());
    m_wsSetting->setWepKeyFlags(NetworkManager::Setting::AgentOwned);
    m_wsSetting->setWepKeyType(NetworkManager::WirelessSecuritySetting::WepKeyType::NotSpecified);
    m_wsSetting->setWepKeyFlags(NetworkManager::Setting::NotRequired);
    m_wsSetting->setAuthAlg(NetworkManager::WirelessSecuritySetting::AuthAlg::None);
    m_wsSetting->setPsk(m_passwdEdit->text());
    m_wirelessSetting->setInitialized(true);
}

void WirelessEditWidget::saveConnSettings()
{
    QDBusPendingReply<> reply;

    if (isHiddenNetWork) {
        if (!ssidInputValid()) {
            return;
        }

        if (m_passwdEdit->text().isEmpty()) {
            setWirelessSettings();
            return;
        } else {
            if (!passwdInputValid()) {
                return;
            }

            setSecurityWirelessSettings();
        }
    } else {
        if (!m_clickedItemWidget->isSecurityNetWork) {
            setWirelessSettings();
            return;
        } else {
            if (!passwdInputValid()) {
                return;
            }

            setSecurityWirelessSettings();
        }
    }

    // 在激活一个新的连接前,需要先取消之前的连接
    for (auto aConn : activeConnections()) {
        for (auto devPath : aConn->devices()) {
            if (devPath == m_devPath) {
                reply = deactivateConnection(aConn->path());
                reply.waitForFinished();
                if (reply.isError()) {
                    qDebug() << "error occurred while deactivate connection" << reply.error();
                }
            }
        }
    }

    for (auto availableItemWidget : m_availableItemWidgets) {
        availableItemWidget->setConnectIconVisible(false);
    }

    // 设置WIFi连接信息
    m_wsSetting->setInitialized(true);

    onBtnClickedHandle();

    Q_EMIT saveSettingsDone();
}

void WirelessEditWidget::prepareConnection()
{
    if (!m_connection) {
        QDBusPendingReply<QDBusObjectPath> reply = addConnection(m_connectionSettings->toMap());
        reply.waitForFinished();
        const QString &connPath = reply.value().path();
        m_connection = findConnection(connPath);
        if (!m_connection) {
            qDebug() << "create connection failed..." << reply.error();
            return;
        }
    }

    Q_EMIT prepareConnectionDone();
}

void WirelessEditWidget::updateConnection()
{
    QDBusPendingReply<> reply;

    // update function saves the settings on the hard disk
    reply = m_connection->update(m_connectionSettings->toMap());
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << "error occurred while updating the connection" << reply.error();
        return;
    }

    if (m_clickedItemWidget->isHiddenNetWork) {
        Q_EMIT activateWirelessConnection(m_ssid, m_connectionUuid);
    }

    reply = activateConnection(m_connection->path(), m_devPath, "");
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << "error occurred while activate connection" << reply.error();
    }
}

void WirelessEditWidget::createConnSettings()
{
    m_connectionSettings = QSharedPointer<NetworkManager::ConnectionSettings>(
                               new NetworkManager::ConnectionSettings(ConnectionSettings::ConnectionType::Wireless));

    m_wsSetting = m_connectionSettings->setting(Setting::SettingType::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();

    m_wirelessSetting = m_connectionSettings->setting(Setting::SettingType::Wireless).staticCast<NetworkManager::WirelessSetting>();

    QString connName = tr("Wireless Connection %1");

    // 设置自动回连
    m_connectionSettings->setAutoconnect(true);

    // 安全选项配置
    m_connectionSettings->setting(Setting::Security8021x).staticCast<NetworkManager::Security8021xSetting>()->setPasswordFlags(Setting::AgentOwned);

    // IP 配置
    m_connectionSettings->setting(Setting::SettingType::Ipv4).staticCast<NetworkManager::Ipv4Setting>();
    m_connectionSettings->setting(Setting::SettingType::Ipv6).staticCast<NetworkManager::Ipv6Setting>();

    if (!connName.isEmpty()) {
        m_connectionSettings->setId(connName.arg(connectionSuffixNum(connName)));
    }
    m_connectionUuid = m_connectionSettings->createNewUuid();
    while (findConnectionByUuid(m_connectionUuid) != nullptr) {
        qint64 second = QDateTime::currentDateTime().toSecsSinceEpoch();
        m_connectionUuid.replace(24, QString::number(second).length(), QString::number(second));
    }
    m_connectionSettings->setUuid(m_connectionUuid);
}

int WirelessEditWidget::connectionSuffixNum(const QString &matchConnName)
{
    if (matchConnName.isEmpty()) {
        return 0;
    }

    NetworkManager::Connection::List connList = listConnections();
    QStringList connNameList;
    int connSuffixNum = 1;

    for (auto conn : connList) {
        if (conn->settings()->connectionType() == ConnectionSettings::ConnectionType::Wireless) {
            connNameList.append(conn->name());
        }
    }

    for (int i = 1; i <= connNameList.size(); ++i) {
        if (!connNameList.contains(matchConnName.arg(i))) {
            connSuffixNum = i;
            break;
        } else if (i == connNameList.size()) {
            connSuffixNum = i + 1;
        }
    }

    return connSuffixNum;
}

void WirelessEditWidget::setClickItem(QStandardItem *clickItem)
{
    m_clickedItem = clickItem;
}

void WirelessEditWidget::setClickItemWidget(WirelessEditWidget *clickItemWidget)
{
    m_clickedItemWidget = clickItemWidget;
}

void WirelessEditWidget::setCurrentActiveConnect(NetworkManager::ActiveConnection::Ptr activeConnect)
{
    m_currentActiveConnect = activeConnect;
}

void WirelessEditWidget::setCurrentConnectItemWidget(WirelessEditWidget *connectItemWidget)
{
    m_connectItemWidget = connectItemWidget;
}

void WirelessEditWidget::setCurrentAvailableItemWidgets(QMap<QString, WirelessEditWidget *> availableItemWidgets)
{
    m_availableItemWidgets = availableItemWidgets;
}

