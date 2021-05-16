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

#include "wirelesswidget.h"
#include "src/global_util/wireless/networkworker.h"
#include "src/global_util/wireless/wirelessitemwidget.h"

#include <QPainter>
#include <DStyle>
#include <DStyleHelper>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QStandardItem>
#include <QStandardItemModel>
#include <DHiDPIHelper>
#include <QtDBus>

#include <networkmanagerqt/manager.h>
#include <networkmanagerqt/accesspoint.h>
#include <networkmanagerqt/settings.h>
#include <networkmanagerqt/connectionsettings.h>

DWIDGET_USE_NAMESPACE
using namespace dtk::wireless;
using namespace NetworkManager;

#define WPAFLAG_KEYMGMT8021X 0x288
#define AP_ITEM_HEIGHT 50
#define EN_US_LOCALE "en_US.UTF-8"

APItem::APItem(QStyle *style, DTK_WIDGET_NAMESPACE::DListView *parent)
    : DStandardItem()
    , m_parentView(nullptr)
    , m_dStyleHelper(style)
    , m_preLoading(false)
    , m_uuid("")
{
    m_parentView = parent;
}

APItem::~APItem()
{
}

void APItem::setSignalStrength(int ss)
{
    if (ss < 0) {
        setIcon(QPixmap());
        return;
    }
    if (5 >= ss)
        setIcon(QIcon::fromTheme(QString("dcc_wireless-%1").arg(0)));
    else if (5 < ss && 30 >= ss)
        setIcon(QIcon::fromTheme(QString("dcc_wireless-%1").arg(2)));
    else if (30 < ss && 55 >= ss)
        setIcon(QIcon::fromTheme(QString("dcc_wireless-%1").arg(4)));
    else if (55 < ss && 65 >= ss)
        setIcon(QIcon::fromTheme(QString("dcc_wireless-%1").arg(6)));
    else if (65 < ss)
        setIcon(QIcon::fromTheme(QString("dcc_wireless-%1").arg(8)));
    APSortInfo si = data(SortRole).value<APSortInfo>();
    si.signalstrength = ss;
    si.ssid = text();
    setData(QVariant::fromValue(si), SortRole);
}

int APItem::signalStrength() const
{
    return data(SortRole).value<APSortInfo>().signalstrength;
}

bool APItem::isConnected()
{
    return checkState();
}

void APItem::setSortInfo(const APSortInfo &si)
{
    setData(QVariant::fromValue(si), SortRole);
}

APSortInfo APItem::sortInfo()
{
    return data(SortRole).value<APSortInfo>();
}

bool APItem::operator<(const QStandardItem &other) const
{
    APSortInfo thisApInfo = data(SortRole).value<APSortInfo>();
    APSortInfo otherApInfo = other.data(SortRole).value<APSortInfo>();
    bool bRet = thisApInfo < otherApInfo;
    return bRet;
}

WirelessPage::WirelessPage(const QString locale, WirelessDevice *dev, QWidget *parent)
    : QWidget(parent)
    , m_isHideNetwork(false)
    , m_localeName(locale)
    , m_sortDelayTimer(new QTimer(this))
    , m_lvAP(new DListView(this))
    , m_modelAP(new QStandardItemModel(m_lvAP))
    , m_titleItem(new APItem(style()))
    , m_device(dev)
    , m_clickedItemWidget(nullptr)
    , m_activingItemWidget(nullptr)
    , m_connectItemWidget(nullptr)
{
    m_preWifiStatus = Wifi_Unknown;
    m_lvAP->setAccessibleName("List_wirelesslist");
    m_lvAP->setModel(m_modelAP);
    m_lvAP->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lvAP->setBackgroundType(DStyledItemDelegate::BackgroundType::ClipCornerBackground);
    m_lvAP->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_lvAP->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_lvAP->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lvAP->setSelectionMode(QAbstractItemView::NoSelection);
    m_lvAP->setViewportMargins(0, 0, 7, 0);

    QScroller *scroller = QScroller::scroller(m_lvAP->viewport());
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(sp);

    m_modelAP->setSortRole(APItem::SortRole);
    m_sortDelayTimer->setInterval(100);
    m_sortDelayTimer->setSingleShot(true);

    if (EN_US_LOCALE == m_localeName) {
        m_lblTitle = new QLabel("Wireless Network");
    } else {
        m_lblTitle = new QLabel(tr("Wireless Network"));
    }

    const QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/img/wireless/refresh.svg");

    // 创建WiFI刷新按钮
    m_loadingIndicator = new DLoadingIndicator;
    m_loadingIndicator->setLoading(false);
    m_loadingIndicator->setSmooth(true);
    m_loadingIndicator->setAniDuration(1000);
    m_loadingIndicator->setAniEasingCurve(QEasingCurve::InOutCirc);
    m_loadingIndicator->installEventFilter(this);
    m_loadingIndicator->setFixedSize(pixmap.size() / devicePixelRatioF());
    m_loadingIndicator->viewport()->setAutoFillBackground(false);
    m_loadingIndicator->setFrameShape(QFrame::NoFrame);
    m_loadingIndicator->setImageSource(pixmap);

    DFontSizeManager::instance()->bind(m_lblTitle, DFontSizeManager::T5, QFont::Light);
    m_switchBtn = new DSwitchButton;

    QHBoxLayout *switchLayout = new QHBoxLayout;
    switchLayout->setContentsMargins(0, 0, 0, 0);
    switchLayout->setMargin(0);
    switchLayout->setSpacing(0);
    switchLayout->addSpacing(3);
    switchLayout->addWidget(m_lblTitle);
    switchLayout->addStretch();
    m_loadingIndicator->setVisible(m_switchBtn->isChecked());
    switchLayout->addWidget(m_loadingIndicator);
    switchLayout->addSpacing(10);
    switchLayout->setContentsMargins(10, 0, 5, 0);
    switchLayout->addWidget(m_switchBtn);

    connect(m_switchBtn, &DSwitchButton::checkedChanged, this, &WirelessPage::onNetworkAdapterChanged);

    m_mainLayout = new QVBoxLayout;
    QWidget *switchWidget = new QWidget(this);
    switchWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    switchWidget->setFixedSize(420, 60);
    switchWidget->setLayout(switchLayout);

    // WiFi列表栏布局
    m_apItems[QString("Title Item")] = m_titleItem;
    m_titleItem->setSortInfo({1000, "", true});
    m_modelAP->appendRow(m_titleItem);
    m_titleItem->setSizeHint(QSize(this->width(), 60));
    m_lvAP->setIndexWidget(m_titleItem->index(), switchWidget);
    m_mainLayout->addWidget(m_lvAP);
    m_layoutCount = m_mainLayout->layout()->count();
    updateLayout(!m_lvAP->isHidden());
    m_mainLayout->setSpacing(10);
    m_mainLayout->setMargin(0);
    m_mainLayout->setStretch(1, 3);

    this->setLayout(m_mainLayout);

    // WiFi AP点击处理
    connect(m_lvAP, &QListView::clicked, this, [this](const QModelIndex & idx) {
        auto item = dynamic_cast<WirelessEditWidget *>(m_lvAP->indexWidget(idx));
        if (item->getConnectIconStatus()) {
            return;
        }

        m_autoConnectHideSsid = "";
        m_clickedItemWidget = item;
        item->setWidgetVisible(true);
        item->setClickItem(m_modelAP->item(idx.row()));
        item->setClickItemWidget(item);
        item->setCurrentAvailableItemWidgets(m_apItemsWidget);
        item->setCurrentConnectItemWidget(m_connectItemWidget);
        item->setCurrentActiveConnect(m_device->activeConnection());
        item->setItemDisplay();
        item->setDevPath(m_device->uni());

        connect(item, &WirelessEditWidget::activateWirelessConnection, this, [this](const QString & ssid, const QString & uuid) {
            Q_UNUSED(uuid);
            m_autoConnectHideSsid = ssid;
        });

        // 在重新选择一个新的AP后，关闭其他AP的编辑框，消除加载状态
        for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
            if (it.value() != item) {
                it.value()->setWidgetVisible(false);
                m_apItems[it.key()]->setSizeHint(QSize(m_apItems[it.key()]->sizeHint().width(), AP_ITEM_HEIGHT));
            }
            it.value()->updateIndicatorDisplay(false);
            emit requestRefreshWiFiStrengthDisplay(WiFiStrengthNoLevel);
        }
    });

    connect(m_sortDelayTimer, &QTimer::timeout, this, &WirelessPage::sortAPList);
    // 监听设备使能信号，做相应逻辑处理
    connect(m_device, &WirelessDevice::enableChanged, this, [this](const bool enabled) {
        m_switchBtn->setChecked(enabled);
        if (m_lvAP) {
            updateWirelessListViewDisplay(enabled);
            updateLayout(!m_lvAP->isHidden());
        }
    });

    connect(m_device, &WirelessDevice::accessPointDisappeared, this, &WirelessPage::onAPRemoved); // 移除WIFI热点信号监听
    connect(m_device, &WirelessDevice::accessPointAppeared, this, &WirelessPage::onAPAdded); // 新增WIFI热点信号监听
    connect(m_device, &WirelessDevice::activeConnectionChanged, this, &WirelessPage::updateActiveAp);

    // init data
    const QStringList mApList = m_device->apList();
    if (!mApList.isEmpty()) {
        for (auto ap : mApList) {
            onAPAdded(ap);
        }
    }

    // 创建隐藏网络Item
    APItem *nonbc = new APItem(style());
    WirelessEditWidget *APWdiget = new WirelessEditWidget(m_localeName, QString("Connect to hidden network"), m_lvAP);
    m_apItems[QString("Connect to hidden network")] = nonbc;
    m_apItemsWidget[QString("Connect to hidden network")] = APWdiget;
    if (EN_US_LOCALE == m_localeName) {
        APWdiget->setHiddenNetWork("Connect to hidden network");
    } else {
        APWdiget->setHiddenNetWork(tr("Connect to hidden network"));
    }

    nonbc->setSortInfo({-1, "", false});
    m_modelAP->appendRow(nonbc);
    nonbc->setSizeHint(QSize(this->width(), AP_ITEM_HEIGHT));
    m_lvAP->setIndexWidget(nonbc->index(), APWdiget);

    QTimer::singleShot(100, this, [ = ] {
        Q_EMIT requestDeviceAPList(m_device->uni());
        Q_EMIT requestWirelessScan();
    });

    updateWirelessListViewDisplay(m_switchBtn->isChecked());
}

WirelessPage *WirelessPage::getInstance(const QString locale, WirelessDevice *device, QWidget *parent)
{
    static WirelessPage w(locale, device, parent);
    return &w;
}

WirelessPage::~WirelessPage()
{
    QScroller *scroller = QScroller::scroller(m_lvAP->viewport());
    if (scroller) {
        scroller->stop();
    }
}

void WirelessPage::setWorker(NetworkWorker *worker)
{
    m_worker = worker;
    m_lvAP->setVisible(true);
    connect(m_worker, &NetworkWorker::deviceEnableChanged, this, [this] { m_switchBtn->setChecked(m_device->enabled()); });
    connect(m_device,
            &Device::stateChanged,
            this,
            &WirelessPage::onDeviceStatusChanged);
    updateLayout(!m_lvAP->isHidden());
    m_switchBtn->setChecked(m_device->enabled());
}

void WirelessPage::onAPAdded(const QString &apPath)
{
    AccessPoint *nmAp = new AccessPoint(apPath);
    const QString &ssid = nmAp->ssid().toUtf8();
    bool isConnect = false;

    if (ssid.isEmpty() || nmAp->rsnFlags() == WPAFLAG_KEYMGMT8021X) {
        qDebug() << "do not show hide network or KeyMgmt8021x network";
        return;
    }

    if (!m_apItems.contains(apPath) && !m_ApList.contains(ssid)) {
        APItem *apItem = new APItem(style(), m_lvAP);

        m_ApList[ssid] = apPath;
        m_modelAP->appendRow(apItem);
        WirelessEditWidget *APWdiget = new WirelessEditWidget(m_localeName, ssid, m_lvAP);

        APWdiget->setItemWidgetInfo(nmAp);
        m_apItems[apPath] = apItem;
        m_apItemsWidget[apPath] = APWdiget;

        if (ssid == m_autoConnectHideSsid) {
            m_autoConnectHideSsid = "";
            APWdiget->updateIndicatorDisplay(true);
            APWdiget->setClickItem(apItem);
            APWdiget->setClickItemWidget(APWdiget);
            APWdiget->updateItemDisplay();
            m_activingItemWidget = APWdiget;
        }

        apItem->setSizeHint(QSize(this->width(), AP_ITEM_HEIGHT));
        if (!APWdiget->m_apPath.isNull()) {
            for (auto conn : activeConnections()) {
                if (conn->state() == ActiveConnection::State::Activated && conn->specificObject() != QString("/")) {
                    isConnect = APWdiget->m_apPath == conn->specificObject();
                    if (isConnect) {
                        m_connectItemWidget = APWdiget;
                        APWdiget->setConnectIconVisible(isConnect);
                    }
                }
            }
        }

        apItem->setSortInfo({nmAp->signalStrength(), ssid, isConnect});
        m_lvAP->setIndexWidget(apItem->index(), APWdiget);

        m_sortDelayTimer->start();
    }
}

void WirelessPage::onAPRemoved(const QString &apPath)
{
    if (!m_apItems.contains(apPath)) return;

    for (auto it = m_ApList.cbegin(); it != m_ApList.cend(); ++it) {
        if (it.value() == apPath) {
            m_ApList.remove(it.key());

            break;
        }
    }

    if (m_apItemsWidget[apPath]->getConnectIconStatus()) {
        emit requestRefreshWiFiStrengthDisplay(WiFiStrengthNoLevel);
    }
    m_modelAP->removeRow(m_modelAP->indexFromItem(m_apItems[apPath]).row());
    m_apItems.remove(apPath);
    m_apItemsWidget.remove(apPath);
}

void WirelessPage::onAPChanged(const QString &apPath)
{
    QString activeApSsid;
    bool isConnect = false;
    AccessPoint *nmAp = new AccessPoint(apPath);
    const QString &ssid = nmAp->ssid();

    if (ssid.isEmpty() || nmAp->rsnFlags() == WPAFLAG_KEYMGMT8021X) {
        qDebug() << "do not show hide network or KeyMgmt8021x network";
        return;
    }

    if (!m_apItems.contains(apPath) && !m_ApList.contains(ssid)) {
        APItem *apItem = new APItem(style(), m_lvAP);
        WirelessEditWidget *APWdiget = new WirelessEditWidget(m_localeName, ssid, m_lvAP);

        m_ApList[ssid] = apPath;
        APWdiget->setItemWidgetInfo(nmAp);
        m_apItems[apPath] = apItem;
        apItem->setSizeHint(QSize(apItem->sizeHint().width(), APWdiget->height()));
        m_lvAP->setIndexWidget(apItem->index(), APWdiget);
        m_apItemsWidget[apPath] = APWdiget;
        m_modelAP->appendRow(apItem);

        if (ssid == m_autoConnectHideSsid) {
            m_autoConnectHideSsid = "";
            APWdiget->updateIndicatorDisplay(true);
            APWdiget->setClickItem(apItem);
            APWdiget->setClickItemWidget(APWdiget);
            APWdiget->updateItemDisplay();
            m_activingItemWidget = APWdiget;
        }

        if (!APWdiget->m_apPath.isNull()) {
            for (auto conn : activeConnections()) {
                if (conn->state() == ActiveConnection::State::Activated && conn->specificObject() != QString("/")) {
                    isConnect = APWdiget->m_apPath == conn->specificObject();
                    if (isConnect) {
                        m_connectItemWidget = APWdiget;
                        APWdiget->setConnectIconVisible(isConnect);
                    }
                }
            }
        }

        apItem->setSortInfo({nmAp->signalStrength(), ssid, isConnect});

        m_sortDelayTimer->start();
        return;
    }

    if (m_apItemsWidget.contains(apPath)) {
        m_apItemsWidget[apPath]->updateItemWidgetDisplay(ssid, nmAp->signalStrength(), nmAp->capabilities());
        m_apItems[apPath]->setSortInfo({nmAp->signalStrength(), ssid, m_apItemsWidget[apPath]->getConnectIconStatus()});
    }

    m_sortDelayTimer->start();
}

void WirelessPage::onActiveAPChanged()
{
    AccessPoint::Ptr activeAp = m_device->activeAccessPoint();

    if (activeAp) {
        if (m_activeAp != activeAp) {
            m_activeAp = activeAp;
        }
    }
}

void WirelessPage::updateWiFiStrengthDisplay()
{
    auto activeConn = m_device->activeConnection();
    int signalStrength = 0;

    for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
        if (activeConn && m_device->state() == Device::State::Activated) {
            if (it.value()->m_apPath == activeConn->specificObject()) {
                signalStrength = it.value()->m_signalStrength;
            }
        }
    }

    WiFiStrenthLevel wifiSignalStrength = WiFiStrengthNoLevel;
    wifiSignalStrength = getWiFiSignalStrengthLevel(signalStrength);
    emit requestRefreshWiFiStrengthDisplay(wifiSignalStrength);
}

void WirelessPage::onDeviceStatusChanged(NetworkManager::Device::State newstate, NetworkManager::Device::State oldstate, NetworkManager::Device::StateChangeReason reason)
{
    Q_UNUSED(oldstate);

    //当wifi状态切换的时候，刷新一下列表，防止出现wifi已经连接，三级页面没有刷新出来的情况，和wifi已经断开，但是页面上还是显示该wifi
    Q_EMIT requestWirelessScan();

    const bool unavailable = newstate <= WirelessDevice::Unavailable;
    if (m_preWifiStatus == Wifi_Unknown) {
        m_preWifiStatus = unavailable ? Wifi_Unavailable : Wifi_Available;
    }
    WifiStatus curWifiStatus = unavailable ? Wifi_Unavailable : Wifi_Available;
    if (curWifiStatus != m_preWifiStatus && newstate > WirelessDevice::Disconnected) {
        m_switchBtn->setChecked(!unavailable);
        onNetworkAdapterChanged(!unavailable);
        m_preWifiStatus = curWifiStatus;
    }
    if (newstate == WirelessDevice::Failed) {
        if (m_clickedItemWidget && m_activingItemWidget) {
            QString ActivingApPath =  m_activingItemWidget->m_apPath;
            if (m_apItemsWidget.contains(ActivingApPath)) {
                m_apItemsWidget[ActivingApPath]->updateIndicatorDisplay(false);
            }

            // 无安全要求的网络连接失败后,不弹提示,去连接之前成功连接的网络
            if (!m_activingItemWidget->isSecurityNetWork) {
                return;
            }

            if (reason == Device::SsidNotFound) {
                m_clickedItemWidget->connectWirelessFailedTips(reason);
            } else {
                connectWirelessErrorHandle(reason);
            }

            emit requestRefreshWiFiStrengthDisplay(WiFiStrengthNoLevel);
        }
    } else if (WirelessDevice::Preparing <= newstate && newstate < WirelessDevice::Activated) {
        for (auto conn : activeConnections()) {
            for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
                if (!it.value()->isHiddenNetWork && conn->uuid() == it.value()->m_connectionUuid) {
                    m_activingItemWidget = it.value();
                    it.value()->updateIndicatorDisplay(true);
                }
            }
        }
    } else if (newstate == WirelessDevice::Activated) {
        auto activeConn = m_device->activeConnection();
        for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
            if (it.value()->m_apPath == activeConn->specificObject()) {
                m_connectItemWidget = it.value();
                it.value()->updateIndicatorDisplay(false);
                it.value()->setConnectIconVisible(true);

                APSortInfo info = m_apItems[it.key()]->sortInfo();
                info.connected = true;
                m_apItems[it.key()]->setSortInfo(info);
                m_sortDelayTimer->start();

                WiFiStrenthLevel wifiSignalStrength = getWiFiSignalStrengthLevel(it.value()->m_signalStrength);
                emit requestRefreshWiFiStrengthDisplay(wifiSignalStrength);
            }
        }
    }
}

void WirelessPage::sortAPList()
{
    m_modelAP->sort(0, Qt::SortOrder::DescendingOrder);
}

void WirelessPage::onNetworkAdapterChanged(bool checked)
{
    Q_EMIT requestDeviceEnabled(m_device->uni(), checked);

    m_loadingIndicator->setVisible(checked);

    if (!m_switchBtn->isChecked()) {
        emit requestRefreshWiFiStrengthDisplay(WiFiStrengthNoNE);
    } else {
        updateWiFiStrengthDisplay();
    }

    if (checked)
        Q_EMIT requestWirelessScan();

    updateLayout(!m_lvAP->isHidden());
    updateWirelessListViewDisplay(checked);
}

void WirelessPage::updateWirelessListViewDisplay(bool checked)
{
    if (!checked) {
        for (auto item : m_apItems) {
            m_lvAP->setRowHidden(item->index().row(), true);
        }

        this->setFixedSize(420, 60);
        m_lvAP->setRowHidden(m_titleItem->index().row(), false);
    } else {
        this->setFixedSize(420, 410);
        for (auto item : m_apItems) {
            m_lvAP->setRowHidden(item->index().row(), false);
        }
    }
}

void WirelessPage::updateLayout(bool enabled)
{
    int layCount = m_mainLayout->layout()->count();
    if (enabled) {
        if (layCount > m_layoutCount) {
            QLayoutItem *layItem = m_mainLayout->takeAt(m_layoutCount);
            if (layItem) {
                delete layItem;
            }
        }
    } else {
        if (layCount <= m_layoutCount) {
            m_mainLayout->addStretch();
        }
    }
    m_mainLayout->invalidate();
}

void WirelessPage::updateActiveAp()
{
    auto activeConn = m_device->activeConnection();

    if (activeConn) {
        for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
            it.value()->setConnectIconVisible(false);
            it.value()->setWidgetVisible(false);
            m_apItems[it.key()]->setSizeHint(QSize(m_apItems[it.key()]->sizeHint().width(), AP_ITEM_HEIGHT));
            it.value()->updateIndicatorDisplay(false);
            if (it.value()->m_apPath == activeConn->specificObject()) {
                m_activingItemWidget = it.value();
                it.value()->updateIndicatorDisplay(true);
                it.value()->setClickItem(m_apItems[it.key()]);
                it.value()->setClickItemWidget(it.value());
            }
        }
    }
}

void WirelessPage::refreshNetwork()
{
    emit requestWirelessScan();

    m_loadingIndicator->setLoading(true);
    const QStringList mApList = m_device->apList();
    if (!mApList.isEmpty()) {
        for (auto ap : mApList) {
            onAPChanged(ap);
        }
    }

    QTimer::singleShot(1000, this, [ = ] {
        m_loadingIndicator->setLoading(false);
    });
}

bool WirelessPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_loadingIndicator) {
        if (event->type() == QEvent::MouseButtonPress) {
            if (!m_loadingIndicator->loading()) {
                refreshNetwork();
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

WiFiStrenthLevel WirelessPage::getWiFiSignalStrengthLevel(int ss)
{
    int signalStrenth = ss;
    WiFiStrenthLevel wifiStrengthLevel = WiFiStrengthNoLevel;
    if (ss < 0) {
        return WiFiStrengthNoLevel;
    }

    if (5 >= signalStrenth)
        wifiStrengthLevel = WiFiStrengthNoLevel;
    else if (5 < signalStrenth && 30 >= signalStrenth)
        wifiStrengthLevel =  WiFiStrengthLOWLevel;
    else if (30 < signalStrenth && 55 >= signalStrenth)
        wifiStrengthLevel = WiFiStrengthMidLevel;
    else if (55 < signalStrenth && 65 >= signalStrenth)
        wifiStrengthLevel = WiFiStrengthMidHighLevel;
    else if (65 < signalStrenth)
        wifiStrengthLevel = WiFiStrengthHighLevel;

    return wifiStrengthLevel;
}

void WirelessPage::connectWirelessErrorHandle(const Device::StateChangeReason &reason)
{
    for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
        if (m_activingItemWidget == it.value()) {
            it.value()->updateIndicatorDisplay(false);
            m_activingItemWidget->connectWirelessFailedTips(reason);
        }
    }
}

void WirelessPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
    painter.setBrush(QBrush(QColor(255, 255, 255, 12)));
    painter.setPen(Qt::transparent);
    QRect rect = this->rect();
    rect.setWidth(rect.width() - 1);
    rect.setHeight(rect.height() - 1);
    painter.drawRoundedRect(rect, 15, 15);
}
