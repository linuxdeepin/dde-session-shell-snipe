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
    , m_requestWirelessScanTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_preWifiStatus = Wifi_Unknown;
    m_lvAP->setAccessibleName("List_wirelesslist");
    m_lvAP->setModel(m_modelAP);
    m_lvAP->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lvAP->setBackgroundType(DStyledItemDelegate::BackgroundType::ClipCornerBackground);
    m_lvAP->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_lvAP->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_lvAP->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lvAP->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lvAP->setSelectionMode(QAbstractItemView::NoSelection);
    m_lvAP->setViewportMargins(0, 0, 7, 0);

    QScroller *scroller = QScroller::scroller(m_lvAP->viewport());
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(sp);

    m_modelAP->setSortRole(APItem::SortRole);
    m_sortDelayTimer->setInterval(100);
    m_sortDelayTimer->setSingleShot(true);

    // add timer
    m_requestWirelessScanTimer->setInterval(60000);
    m_requestWirelessScanTimer->setSingleShot(false);

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

        // 如果当前AP正在激活或者已经连接的AP, 不做处理
        if (!item || (!item->isHiddenNetWork && item->getIndicatorStatus()) || item->getConnectIconStatus())
            return;

        m_isHideNetwork = false;

        // 在重新选择一个新的AP后，关闭其他AP的编辑框，消除加载和连接的状态,并更新排序的连接状态
        for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
            if (it.value() != item) {
                it.value()->setWidgetVisible(false);
                APSortInfo info = m_apItems[it.key()]->sortInfo();
                info.connected = false;
                m_apItems[it.key()]->setSortInfo(info);
                m_apItems[it.key()]->setSizeHint(QSize(m_apItems[it.key()]->sizeHint().width(), AP_ITEM_HEIGHT));
            }
            it.value()->updateIndicatorDisplay(false);
            it.value()->setConnectIconVisible(false);
        }

        m_autoConnectHideSsid = "";
        m_autoConnectHideUuid = "";
        m_clickedItemWidget = item;
        item->setWidgetVisible(true);
        item->setClickItem(m_modelAP->item(idx.row()));
        item->setClickItemWidget(item);
        item->requestApConnect();

        connect(item, &WirelessEditWidget::activateWirelessConnection, this, [this](const QString & ssid, const QString & uuid) {
            m_autoConnectHideUuid = uuid;
            m_autoConnectHideSsid = ssid;
        });

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

    connect(m_device, &WirelessDevice::accessPointAppeared, this, &WirelessPage::onAPAdded); // 新增WIFI热点信号监听
    connect(m_device, &WirelessDevice::accessPointDisappeared, this, &WirelessPage::onAPRemoved); // 移除WIFI热点信号监听
    connect(m_device, &WirelessDevice::activeConnectionChanged, this, &WirelessPage::updateActiveAp);
    // add timer scan
    connect(m_requestWirelessScanTimer, &QTimer::timeout, this, [ = ] {
        Q_EMIT requestDeviceAPList(m_device->path());
        Q_EMIT requestWirelessScan();
    });

    // init data
    const QStringList mApList = m_device->accessPoints();
    if (!mApList.isEmpty()) {
        for (auto ap : mApList) {
            onAPAdded(ap);
        }
    }

    // 创建隐藏网络Item
    APItem *nonbc = new APItem(style());
    WirelessEditWidget *APWdiget = new WirelessEditWidget(m_device, m_localeName, QString("Connect to hidden network"), m_lvAP);
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

    // start timer
    m_requestWirelessScanTimer->start();

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
    // stop timer
    m_requestWirelessScanTimer->stop();
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

    if (ssid.isEmpty()) {
        qDebug() << "do not show hide network";
        return;
    }

    if (!m_apItems.contains(ssid) && !m_ApList.contains(ssid)) {
        APItem *apItem = new APItem(style(), m_lvAP);

        m_ApList[ssid] = apPath;
        m_modelAP->appendRow(apItem);
        WirelessEditWidget *APWdiget = new WirelessEditWidget(m_device, m_localeName, ssid, m_lvAP);

        APWdiget->setItemWidgetInfo(nmAp);
        m_apItems[ssid] = apItem;
        m_apItemsWidget[ssid] = APWdiget;

        if (ssid == m_autoConnectHideSsid) {
            m_isHideNetwork = true;
            APWdiget->m_connectionUuid = m_autoConnectHideUuid;
            m_autoConnectHideUuid = "";
            m_autoConnectHideSsid = "";
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

    // 根据wifi开关的状态去更新显示
    updateWirelessListViewDisplay(m_switchBtn->isChecked());
}

QString WirelessPage::getSsidByApPath(const QString &path)
{
    QString ssid;
    if (!path.isEmpty()) {
        AccessPoint *nmAp = new AccessPoint(path);
        ssid = nmAp->ssid().toUtf8();
    }

    return ssid;
}

void WirelessPage::onAPRemoved(const QString &apPath)
{
    QString ssid;

    for (auto it = m_ApList.cbegin(); it != m_ApList.cend(); ++it) {
        if (it.value() == apPath) {
            ssid = it.key();
            m_ApList.remove(it.key());

            break;
        }
    }

    // 移除的是我们点击的或者正在激活的Ap item, 需要更新对应item Widget的状态
    for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
        if (it.key() == ssid) {
            if (m_connectItemWidget && m_connectItemWidget == it.value()) {
                // check if removed ap is connected ap
                m_connectItemWidget->setConnectIconVisible(false);
                m_connectItemWidget = nullptr;
                break;
            }

            if (m_activingItemWidget == it.value()) {
                m_activingItemWidget = nullptr;
                m_clickedItemWidget = nullptr;

                break;
            }
        }
    }

    if (!ssid.isEmpty()) {
        m_modelAP->removeRow(m_modelAP->indexFromItem(m_apItems[ssid]).row());
        m_apItems.remove(ssid);
        m_apItemsWidget.remove(ssid);
    }
}

void WirelessPage::onAPChanged(const QString &apPath)
{
    AccessPoint *nmAp = new AccessPoint(apPath);
    const QString &ssid = nmAp->ssid();

    if (ssid.isEmpty()) {
        qDebug() << "do not show hide network";
        return;
    }

    if (m_ApList.contains(ssid) && m_apItemsWidget.contains(ssid)) {
        m_apItemsWidget[ssid]->updateItemWidgetDisplay(ssid, nmAp->signalStrength(), nmAp->capabilities());
        m_apItems[ssid]->setSortInfo({nmAp->signalStrength(), ssid, m_apItemsWidget[ssid]->getConnectIconStatus()});
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

void WirelessPage::onDeviceStatusChanged(NetworkManager::Device::State newstate, NetworkManager::Device::State oldstate, NetworkManager::Device::StateChangeReason reason)
{
    Q_UNUSED(oldstate);

    // 每次去请求一个新的连接时，networkmanager会先deactive之前的连接，然后去请求新的连接
    // 此时我们应该更新对应连接的显示状态
    if ((oldstate == WirelessDevice::Activated) && (newstate == WirelessDevice::Deactivating) && (reason == Device::NewActivation)){
        for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
            if (it.value()->getConnectIconStatus()){
                it.value()->setConnectIconVisible(false);
            }
        }
    }

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
        if (m_activingItemWidget) {
            m_isHideNetwork = false;

            // 连接失败, 取消连接加载的状态
            for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
                it.value()->updateIndicatorDisplay(false);
            }

            // 无安全要求的网络(非隐藏网络)连接失败后,不弹提示,去连接之前成功连接的网络
            if (!m_activingItemWidget->isSecurityNetWork) {
                if (!m_clickedItemWidget) {
                    return;
                } else {
                    if (!m_clickedItemWidget->isHiddenNetWork) {
                        return;
                    }
                }
            }

            if (m_clickedItemWidget && (reason == Device::SsidNotFound) && (m_clickedItemWidget->isHiddenNetWork)) {
                m_clickedItemWidget->connectWirelessFailedTips(reason);
            } else {
                connectWirelessErrorHandle(reason);
            }
        }
    } else if (WirelessDevice::Preparing <= newstate && newstate < WirelessDevice::Activated) {
        for (auto conn : activeConnections()) {
            for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
                if (!it.value()->isHiddenNetWork && conn->uuid() == it.value()->m_connectionUuid) {
                    m_activingItemWidget = it.value();
                    it.value()->setClickItem(m_apItems[it.key()]);
                    it.value()->setClickItemWidget(m_activingItemWidget);
                    it.value()->updateIndicatorDisplay(true);
                }
            }
        }
    } else if (newstate == WirelessDevice::Activated) {
        m_isHideNetwork = false;
        auto activeAp = m_device->activeAccessPoint();
        if (activeAp) {
            for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
                if (it.key() == getSsidByApPath(activeAp->uni())) {
                    m_connectItemWidget = it.value();
                    it.value()->updateIndicatorDisplay(false);
                    it.value()->setConnectIconVisible(true);

                    if (activeAp) {
                        it.value()->updateItemWidgetDisplay(activeAp->ssid(), activeAp->signalStrength(), activeAp->capabilities());
                        m_apItems[it.key()]->setSortInfo({activeAp->signalStrength(), activeAp->ssid(), true});
                    }

                    m_sortDelayTimer->start();
                }
            }
        }
    } else if (newstate == WirelessDevice::Disconnected) {
        if (m_connectItemWidget) {
            m_connectItemWidget->setConnectIconVisible(false);
            m_connectItemWidget = nullptr;
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

        m_lvAP->setRowHidden(m_titleItem->index().row(), false);
    } else {
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
            if (!m_apItems.contains(it.key()))
                return;
            it.value()->setConnectIconVisible(false);
            it.value()->setWidgetVisible(false);
            APSortInfo info = m_apItems[it.key()]->sortInfo();
            info.connected = false;
            m_apItems[it.key()]->setSortInfo(info);
            it.value()->updateIndicatorDisplay(false);

            m_apItems[it.key()]->setSizeHint(QSize(m_apItems[it.key()]->sizeHint().width(), AP_ITEM_HEIGHT));
            it.value()->updateIndicatorDisplay(false);
            if (it.key() == getSsidByApPath(activeConn->specificObject())) {
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

    const QStringList mApList = m_device->accessPoints();
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

void WirelessPage::connectWirelessErrorHandle(const Device::StateChangeReason &reason)
{
    for (auto it = m_apItemsWidget.cbegin(); it != m_apItemsWidget.cend(); ++it) {
        if (m_activingItemWidget && m_clickedItemWidget) {
            // 激活的Ap item存在且是点击的Ap, 才去弹错误提示
            if (m_activingItemWidget == it.value() && m_clickedItemWidget == m_activingItemWidget) {
                m_activingItemWidget->connectWirelessFailedTips(reason);
            }
        }
    }
}
