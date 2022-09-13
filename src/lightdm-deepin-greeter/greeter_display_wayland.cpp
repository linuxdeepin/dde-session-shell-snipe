// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeter_display_wayland.h"

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <linux/input-event-codes.h>

#include <KF5/KWayland/Client/registry.h>
#include <KF5/KWayland/Client/event_queue.h>
#include <KF5/KWayland/Client/connection_thread.h>
#include <KF5/KWayland/Client/outputdevice.h>
#include <KF5/KWayland/Client/outputconfiguration.h>
#include <KF5/KWayland/Client/outputmanagement.h>

#include <QThread>
#include <QDebug>
#include <QRect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusPendingCall>
#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QCryptographicHash>
#include <QtCore/qmath.h>
#include <QTimer>

static QMap<QString, MonitorConfig> MonitorConfigsForUuid_v1;

GreeterDisplayWayland::GreeterDisplayWayland(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
    , m_eventQueue(nullptr)
    , m_pConf(nullptr)
    , m_pManager(nullptr)
    , m_displayMode(Extend_Mode)
    , m_setOutputTimer(new QTimer(this))
{
    QDBusInterface displayConfig("com.deepin.system.Display",
                                                 "/com/deepin/system/Display",
                                                 "com.deepin.system.Display",
                                                QDBusConnection::systemBus(), this);
    QDBusPendingCall configReply = displayConfig.asyncCall("GetConfig");

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(configReply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, configReply, watcher] {
        if (!configReply.isError()) {
            QDBusReply<QString> reply = configReply.reply();
                QString config = reply.value();
                QJsonParseError jsonError;
                QJsonDocument jsonDoc (QJsonDocument::fromJson(config.toStdString().data(), &jsonError));
                if (jsonError.error == QJsonParseError::NoError) {
                    QJsonObject rootObj = jsonDoc.object();
                    QJsonObject Config = rootObj.value("Config").toObject();
                    m_screensObj = Config.value("Screens").toObject();
                    m_displayMode = Config.value("DisplayMode").toInt();
                    qDebug() << "m_displayMode :" << m_displayMode;
                }
        } else {
            qDebug() << "Failed to get license state: " << configReply.error().message();
        }
        watcher->deleteLater();
    });
    // 加延时为了等待窗管发送完所有屏幕添加或移除信号
    m_setOutputTimer->setInterval(100);
    connect(m_setOutputTimer, &QTimer::timeout, this, [this] {
        setOutputs();
    });
}

GreeterDisplayWayland::~GreeterDisplayWayland()
 {
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
 }

void GreeterDisplayWayland::start()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this, [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);
            Registry *registry = new Registry(this);
            setupRegistry(registry);
        }, Qt::QueuedConnection
    );
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();
    m_connectionThreadObject->initConnection();
}

void GreeterDisplayWayland::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::outputManagementAnnounced, this, [ this, registry ](quint32 name, quint32 version) {
        qDebug() << "onMangementAnnounced";

        if (!m_pManager) {
            m_pManager = registry->createOutputManagement(name, version, this);
            if (!m_pManager || !m_pManager->isValid()) {
                qDebug() << "create manager is nullptr or not valid!";
                return;
            }
            if (!m_pConf) {
                m_pConf = m_pManager->createConfiguration();
                if (!m_pConf || !m_pConf->isValid()) {
                    qDebug() << "create output configure is null or is not vaild";
                    return;
                }
                disconnect(m_pConf, &OutputConfiguration::applied, this, nullptr);
                disconnect(m_pConf, &OutputConfiguration::failed, this, nullptr);
                connect(m_pConf, &OutputConfiguration::applied, [this] {
                    qDebug() << "Configuration applied!";
                    Q_EMIT setOutputFinished();
                });
                connect(m_pConf, &OutputConfiguration::failed, [this] {
                    qDebug() << "Configuration failed!";
                    Q_EMIT setOutputFinished();
                });
            }
        }
    });

    connect(registry, &Registry::outputDeviceAnnounced, this, [ this, registry ](quint32 name, quint32 version) {
        qDebug() << "outputDeviceAnnounced";

        auto dev = registry->createOutputDevice(name, version);
        if (!dev || !dev->isValid())
        {
            qDebug() << "get dev is null or not valid!";
            return;
        }

        connect(dev, &OutputDevice::changed, this, [=] {
            this->onDeviceChanged(dev);
        });

        connect(dev, &OutputDevice::removed, this, [dev, this] {
            qDebug() << "OutputDevice::removed ...";
            if (m_displayMode == Mirror_Mode) {
                Q_EMIT setOutputStart();
            }
            MonitorConfigsForUuid_v1.remove(dev->uuid());
            // 登录界面只有插拔需要处理
            if (m_removeUuid != dev->uuid()) {
                if (!m_setOutputTimer->isActive()) {
                    m_setOutputTimer->start();
                }
                m_removeUuid = dev->uuid();
                qDebug() << "update m_removeUuid--->" << m_removeUuid;
            }
        });
    });

    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void GreeterDisplayWayland::onDeviceChanged(OutputDevice *dev)
{
    qDebug() << "onDeviceChanged ...";
    qDebug()<<"[Changed]: "<<dev->model()<<dev->uuid()<<dev->globalPosition()<<dev->geometry()<<dev->refreshRate()<<dev->pixelSize();
    QString uuid = dev->uuid();
    QPoint point = dev->globalPosition();
    if (MonitorConfigsForUuid_v1.find(uuid) == MonitorConfigsForUuid_v1.end()) {
        qDebug() << "OutputDevice::Added uuid --->" << uuid;
        if (m_displayMode == Mirror_Mode) {
            Q_EMIT setOutputStart();
        }
        QString name = getOutputDeviceName(dev->model(), dev->manufacturer());
        QString stdName = getStdMonitorName(QByteArray(dev->edid(), dev->edid().size()));
        MonitorConfig cfg;
        cfg.uuid = getOutputUuid(name, stdName, dev->edid());
        cfg.dev = dev;
        MonitorConfigsForUuid_v1.insert(uuid, cfg);
        // 登录界面只有插拔需要处理
        if (!m_setOutputTimer->isActive()) {
            m_setOutputTimer->start();
        }
        if (dev->uuid() == m_removeUuid) {
            qDebug() << "reset removeUuid ...";
            m_removeUuid = "";
        }
    } else {
        // 存在设置之后不生效的情况，需要根据changed信号判断，暂不处理，可能会一直设置
        qDebug() << "OutputDevice::changed ..." << uuid;
        if (MonitorConfigsForUuid_v1[uuid].hasCof) {
            bool enabled = OutputDevice::Enablement::Enabled == dev->enabled();
            if ((MonitorConfigsForUuid_v1[uuid].x != point.x() && MonitorConfigsForUuid_v1[uuid].y != point.y()) ||
                MonitorConfigsForUuid_v1[uuid].enabled != enabled) {
                qWarning() << "apply do not work...";
            }
        }
    }
}

void GreeterDisplayWayland::setOutputs()
{
    if (m_setOutputTimer->isActive()) {
        m_setOutputTimer->stop();
    }
    if (MonitorConfigsForUuid_v1.size() == 0) {
        qDebug() << "has no monitors ...";
        return;
    }
    QString uuidKey;
    QMap<QString, MonitorConfig>::const_iterator it = MonitorConfigsForUuid_v1.begin();
    while (it != MonitorConfigsForUuid_v1.end())
    {
        uuidKey = uuidKey + it.value().uuid + ",";
        it++;
    }
    uuidKey.remove(uuidKey.length() - 1, 1);
    qDebug() << "uuidKey:--->" << uuidKey;
    QString displayMode;
    switch (m_displayMode)
    {
        case Custom_Mode:
            displayMode = "";
            break;
        case Mirror_Mode:
            displayMode = "Mirror";
            break;
        case Extend_Mode:
            displayMode = "Extend";
            break;
        case Single_Mode:
            displayMode = "OnlyOneMap";
            break;
    }
    // 先找到uuidkey
    if (m_screensObj.value(uuidKey).toObject().value(displayMode).toObject().isEmpty()) {
        QStringList uuidList = uuidKey.split(",");
        if (uuidList.size() > 1) {
            // 遍历所有组合，找到系统配置文件中对应组合的key
            int uuidIndexArry[uuidList.size()];
            for(int i = 0; i < uuidList.size(); i++) {
                uuidIndexArry[i] = i;
            }
            do {
                QString uuidKeyTmp;
                for(int i = 0; i < uuidList.size(); i++) {
                    uuidKeyTmp = uuidKeyTmp + uuidList[uuidIndexArry[i]] + ",";
                }
                uuidKeyTmp.remove(uuidKeyTmp.length() - 1, 1);
                if (!m_screensObj.value(uuidKeyTmp).toObject().value(displayMode).toObject().isEmpty()) {
                    qDebug() << "uuidKey update--->" << uuidKeyTmp;
                    uuidKey = uuidKeyTmp;
                    break;
                }
            } while (std::next_permutation(uuidIndexArry, uuidIndexArry + uuidList.size()));
        }
    }
    QJsonArray monitorsArr;
    if (MonitorConfigsForUuid_v1.size() == 1) {
        qDebug() << "Single mode ...";
        monitorsArr = m_screensObj.value(uuidKey).toObject().value("Single").toObject().value("Monitors").toArray();
    } else {
        if (m_displayMode != Single_Mode) {
            monitorsArr = m_screensObj.value(uuidKey).toObject().value(displayMode).toObject().value("Monitors").toArray();
        } else {
            QString enableUuid = m_screensObj.value(uuidKey).toObject().value("OnlyOneUuid").toString();
            qDebug() << "enableUuid--->" << enableUuid;
            monitorsArr = m_screensObj.value(uuidKey).toObject().value(displayMode).toObject().value(enableUuid).toObject().value("Monitors").toArray();
            QStringList uuidList = uuidKey.split(",");
            if (uuidList.size() > 1) {
                // 仅单屏的时候，根据OnlyOneUuid找到需要点亮的屏幕放在list第一个，disable的屏幕依次放在后面，
                // disable屏幕的属性在下面初始化配置时修改
                foreach(QString uuid, uuidList) {
                    if(uuid != enableUuid) {
                        QJsonValue valueTmp = m_screensObj.value(uuidKey).toObject().value(displayMode).toObject().value(uuid).toObject().value("Monitors").toArray()[0];
                        if (valueTmp.toObject().isEmpty()) {
                            qDebug() << uuid << ":valueTmp is null";
                            // 仅单屏，若另一个屏幕没有配置，给个默认配置
                            QJsonObject obj;
                            obj["UUID"] = uuid;
                            obj["Enabled"] = false;
                            obj["X"] = 0;
                            obj["Y"] = 0;
                            obj["Rotation"] = 1;
                            valueTmp = obj;
                        }
                        monitorsArr.append(valueTmp);
                        qDebug() << "disableUuid--->" << uuid;
                    }
                }
            }
        }
    }

    if (!monitorsArr.isEmpty()) {
        // 按系统配置设置
        for (int i = 0; i < monitorsArr.size(); i++) {
            QJsonObject jsonMonitorConfig = monitorsArr.at(i).toObject();
            QString uuid = jsonMonitorConfig.value("UUID").toString();
            QString id;
            for(QMap<QString,MonitorConfig>::Iterator it = MonitorConfigsForUuid_v1.begin(); it != MonitorConfigsForUuid_v1.end(); ++it) {
                if(it.value().uuid== uuid) {
                    id = it.key();
                }
            }
            if (id.isEmpty()) {
                qDebug() << "invalid monitor ...";
                break;
            }
            MonitorConfigsForUuid_v1[id].x = jsonMonitorConfig.value("X").toInt();
            MonitorConfigsForUuid_v1[id].y = jsonMonitorConfig.value("Y").toInt();
            MonitorConfigsForUuid_v1[id].name = jsonMonitorConfig.value("Name").toString();
            MonitorConfigsForUuid_v1[id].width = jsonMonitorConfig.value("Width").toInt();
            MonitorConfigsForUuid_v1[id].height = jsonMonitorConfig.value("Height").toInt();
            MonitorConfigsForUuid_v1[id].refresh_rate = jsonMonitorConfig.value("RefreshRate").toDouble();
            // 使用qRound四舍五入，在klv中greeter在计算旋转角度时，rotation=8时，使用double接收计算结果为3,用int接收计算结果为2
            // 但是写demo计算并不会有问题，暂时找不到根因，详见bug158393
            MonitorConfigsForUuid_v1[id].transform = qRound(qLn(jsonMonitorConfig.value("Rotation").toInt()) / qLn(2));
            MonitorConfigsForUuid_v1[id].brightness = jsonMonitorConfig.value("Brightness").toDouble();
            MonitorConfigsForUuid_v1[id].primary = jsonMonitorConfig.value("Primary").toBool();
            // 根据是否是仅单屏显示，决定是否从配置文件中读取enable属性
            if (m_displayMode == Single_Mode && i > 0) {
                MonitorConfigsForUuid_v1[id].enabled = false;
            } else {
                MonitorConfigsForUuid_v1[id].enabled = jsonMonitorConfig.value("Enabled").toBool();
            }
            applyConfig(id);
        }
    } else {
        // 默认设置
        applyDefault();
    }
}

QSize GreeterDisplayWayland::commonSizeForMirrorMode()
{
    QVector<QVector<QSize>> modesVec;
    // 对所有分辨率组合去重
    foreach (MonitorConfig cfg, MonitorConfigsForUuid_v1) {
        QVector<QSize> modeVecTmp;
        auto mode = cfg.dev->modes();
        for (int i = 0; i < mode.size(); i++)
        {
            if (!modeVecTmp.contains(mode[i].size)) {
                modeVecTmp.push_back(mode[i].size);
            }
        }
        modesVec.push_back(modeVecTmp);
    }
    // 找出相同分辨率的所有组合
    QVector<QSize> commonModeVec;
    for (int i = 0; i < modesVec[0].size(); i++)
    {
        bool isContain = true;
        foreach (QVector<QSize> vec, modesVec) {
            if (!vec.contains(modesVec[0][i])) {
                isContain = false;
                break;
            }
        }
        if (isContain) {
            commonModeVec.push_back(modesVec[0][i]);
        }
    }
    // 没有配置文件，默认找出共同最高分辨率
    int product = 0;
    int index = 0;
    for (int i = 0; i < commonModeVec.size(); i++) {
        int tmp = commonModeVec[i].width() * commonModeVec[i].height();
        if (tmp > product) {
            product = tmp;
            index = i;
        }
    }
    return commonModeVec[index];
}

void GreeterDisplayWayland::applyDefault()
{
    qDebug() << "applyDefault ...";
    QSize applySize;
    if (m_displayMode == Mirror_Mode) {
       applySize = commonSizeForMirrorMode();
    }
    QPoint o(0, 0);
    bool enabled = true;
    foreach (MonitorConfig cfg, MonitorConfigsForUuid_v1) {
        auto dev = cfg.dev;
        if (m_displayMode != Mirror_Mode) {
            applySize = dev->geometry().size();
        }
        qDebug() << "applyDefault uuid--->" << dev->uuid() << "applySize--->" << applySize;
        for (auto m : dev->modes()) {
            if (m.size.width() == applySize.width() && m.size.height() == applySize.height()) {
                qDebug() << "set output mode :" << m.size.width() << "x" << m.size.height() << "and refreshRate :" << m.refreshRate;
                m_pConf->setMode(dev, m.id);
                break;
            }
        }
        qDebug() << "set output setPosition to " << o.x() << o.y();
        m_pConf->setPosition(dev, o);
        qDebug() << "set output setEnabled to " << enabled;
        m_pConf->setEnabled(dev, OutputDevice::Enablement(enabled));
        qDebug() << "set output transform to 0";
        m_pConf->setTransform(dev, OutputDevice::Transform(0));
        m_pConf->apply();
        // 找不到配置文件，如果是扩展模式，默认横向扩展，如果是仅单屏默认点亮第一个屏幕
        if (m_displayMode == Extend_Mode) {
            o += QPoint(dev->geometry().width(), 0);
        }
        if (m_displayMode == Single_Mode) {
            enabled = false;
        }
    }
}

void GreeterDisplayWayland::applyConfig(QString uuid)
{
    qDebug() << "applyConfig ...";
    auto monitor = MonitorConfigsForUuid_v1[uuid];
    auto dev = monitor.dev;
    bool modeSet = false;
    for (auto m : dev->modes()) {
        if (m.size.width() == monitor.width && m.size.height() == monitor.height && m.refreshRate == monitor.refresh_rate * 1000) {
            qDebug() << "set output mode :" << monitor.width << "x" << monitor.height << "and refreshRate :" << monitor.refresh_rate;
            m_pConf->setMode(dev, m.id);
            modeSet = true;
            break;
        }
    }
    // 如果没有对应的组合，则设置第一个
    if (!modeSet) {
        m_pConf->setMode(dev, dev->modes()[0].id);
    }
    qDebug() << "set output setPosition to " << monitor.x << monitor.y;
    m_pConf->setPosition(dev, QPoint(monitor.x, monitor.y));
    qDebug() << "set output setEnabled to " << monitor.enabled;
    m_pConf->setEnabled(dev, OutputDevice::Enablement(monitor.enabled));
    qDebug() << "set output transform to " << monitor.transform;
    m_pConf->setTransform(dev, OutputDevice::Transform(monitor.transform));
    m_pConf->apply();
    MonitorConfigsForUuid_v1[uuid].hasCof = true;
}

QString GreeterDisplayWayland::getStdMonitorName(QByteArray edid)
{
    QRegularExpression re("^card\\d\\-.*");
    QDir dir("/sys/class/drm");
    if(!dir.exists()) {
        return "";
    }
    QStringList names = dir.entryList(QDir::Dirs);
    for(auto name : names) {
        QRegularExpressionMatch match = re.match(name);
        if (match.hasMatch()) {
            QString nameParts;
            int index = name.indexOf("-");
            if (index != -1) {
                nameParts = name.mid(index + 1);
            }
            QFile file("/sys/class/drm/" + name + "/edid");
            file.open(QIODevice::ReadOnly);
            QByteArray sysEdid = file.readAll();
            if (!nameParts.isEmpty() && sysEdid.length() > 0 && edidEqual(edid, sysEdid)) {
                return nameParts;
            }
        }
    }
    return "";
}

bool GreeterDisplayWayland::edidEqual(QByteArray edid1, QByteArray edid2)
{
    if (edid1.length() == edid2.length()) {
        return edid1 == edid2;
    }
    if (edid2.length() > 128 && edid1.length() == 128) {
        edid2.truncate(128);
        return edid1 == edid2;
    }
    return false;
}

QString GreeterDisplayWayland::getOutputUuid(QString name, QString stdName, QByteArray edid)
{
    if (edid.length() < 128) {
        return name + "||v1";
    }
    edid.truncate(128);
    QString id;
    QCryptographicHash md(QCryptographicHash::Md5);
    md.addData(edid);
    QByteArray tmp = md.result();
    id.append(tmp.toHex());
    if (id == "") {
        return name + "||v1";
    }

    if (!stdName.isEmpty()) {
        name = "@" + stdName;
    }

    return name + "|" + id + "|v1";
}

// 根据 model 和 make/Manufacturer 获取显示器的名字
// 比如 model 'eDP-1-未知', make 'LP140WH8-TPD' => eDP-1
// 'eDP-1-dell', 'dell' => eDP-1   这个例子可能目前不真实
// 'HDMI-A-2-VA2430-H-3/W72211325199', 'VSC' => HDMI-A-2
QString GreeterDisplayWayland::getOutputDeviceName(const QString& model, const QString& make)
{
    QString preMake = make.split(" ")[0];
    QString name = model.split(preMake)[0];
    if (name.right(1) == "-") {
        name = name.left(name.size() -1);
    }
    if (name != model) {
        return name;
    }
    QStringList nameList = model.split("-");
    if (nameList.size() <= 2) {
        int index = model.indexOf(' ');
        if (index == -1) {
            return model;
        }
        return model.left(index);
    }
    QStringList result;
    // 找到第一个纯数字部分
    for (int i = 0; i < nameList.size(); ++i) {
        bool ok = false;
        nameList[i].toInt(&ok, 10);
        if (ok && i >= 1) {
            // name 是数字
            // 比如 model 为 HDMI-A-2-VA2430-H-3/W72211325199
            // 可以得到 HDMI-A-2 这个名字
            for (int j = 0; j < i + 1; ++j) {
                result.append(nameList[j]);
            }
            return result.join("-");
        }
    }

    int idx = nameList.size() - 1;
    for(; idx > 1; idx--) {
        if (nameList[idx].length() > 1) {
            continue;
        }
        break;
    }
    // 比如 model 为 HDMI-A-2-VA2430-H-3/W72211325199
    // 可以得到 HDMI-A-2-VA2430-H 这个名字
    for (int j = 0; j < idx + 1; ++j) {
        result.append(nameList[j]);
    }
    return result.join("-");
}