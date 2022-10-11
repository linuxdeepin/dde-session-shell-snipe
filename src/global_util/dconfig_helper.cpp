// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfig_helper.h"
#include "public_func.h"

#include <QDebug>
#include <QMetaMethod>
#include <qdebug.h>

DCORE_USE_NAMESPACE

Q_GLOBAL_STATIC(DConfigHelper, dConfigWatcher)

DConfigHelper::DConfigHelper(QObject *parent)
    : QObject(parent)
{
    initializeDConfig(getDefaultConfigFileName(), getDefaultConfigFileName(), "");
}

DConfigHelper* DConfigHelper::instance()
{
    return dConfigWatcher;
}

DConfig* DConfigHelper::initializeDConfig(const QString &appId, const QString &name, const QString &subpath)
{
    DConfig* dConfig = DConfig::create(appId, name, subpath, this);
    if (!dConfig) {
        qWarning() << "Create DConfig failed, appId: " << appId << ", name: " << name << ", subpath: " << subpath;
        return nullptr;
    }

    m_dConfigs[packageDConfigPath(appId, name, subpath)] = dConfig;
    m_bindInfos[dConfig] = {};

    // 即时响应数据变化
    connect(dConfig, &DConfig::valueChanged, this, [this, dConfig] (const QString &key) {
        const QVariant &value = dConfig->value(key);
        auto it = m_bindInfos.find(dConfig);
        if (it == m_bindInfos.end())
            return;

        QMap<QString, QList<QObject *>> &bindInfo = it.value();
        auto bindInfoIt = bindInfo.find(key);
        if (bindInfoIt != bindInfo.end()) {
            for (QObject *obj : bindInfoIt.value()) {
                QMetaObject::invokeMethod(obj, "OnDConfigPropertyChanged", Qt::QueuedConnection, Q_ARG(QString, key), Q_ARG(QVariant,value));
            }
        }
    });

    return dConfig;
}

void DConfigHelper::bind(QObject *obj, const QString &key)
{
    if (!obj)
        return;

    DConfig *dConfig = defaultDConfigObject();
    if (!dConfig)
        return;

    const QMetaObject *metaObject = obj->metaObject();
    if (-1 == metaObject->indexOfMethod("OnDConfigPropertyChanged(QString,QVariant)")) {
        qWarning() << "Bind dconfig failed, " + QString(metaObject->className()) + " does not have `OnDConfigPropertyChanged` function";
        return;
    }

    auto it = m_bindInfos.find(dConfig);
    if (it == m_bindInfos.end())
        return;

    QMap<QString, QList<QObject *>> &bindInfo = it.value();
    auto bindInfoIt = bindInfo.find(key);
    if (bindInfoIt != bindInfo.end()) {
        bindInfoIt.value().append(obj);
    } else {
        bindInfo[key] = {obj};
    }

    connect(obj, &QObject::destroyed, this, [this, obj] {
        unBind(obj);
    });
}

DConfig* DConfigHelper::defaultDConfigObject()
{
    return dConfigObject(getDefaultConfigFileName(), getDefaultConfigFileName(), "");
}

void DConfigHelper::unBind(QObject *obj, const QString &key)
{
    qInfo() << Q_FUNC_INFO << obj << ", key: " << key;
    if (!obj) {
        qWarning() << "Unbinding object is null";
        return;
    }

    auto it = m_bindInfos.begin();
    for(; it != m_bindInfos.end(); ++it) {
        auto it1 = it.value().begin();
        for (; it1 != it.value().end(); ++it1) {
            if (!key.isEmpty() && it1.key() != key)
                continue;

            it1.value().removeAll(obj);
        }
    }
}

/**
 * @brief Get config from default dconfig
 */
QVariant DConfigHelper::getConfig(const QString &key, const QVariant &defaultValue)
{
    return getConfig(getDefaultConfigFileName(), getDefaultConfigFileName(), "", key, defaultValue);
}

/**
 * @brief Set config to default dconfig
 */
void DConfigHelper::setConfig(const QString &key, const QVariant &value)
{
    return setConfig(getDefaultConfigFileName(), getDefaultConfigFileName(), "", key, value);
}

QVariant DConfigHelper::getConfig(const QString &appId, const QString &name, const QString &subpath, const QString &key, const QVariant &defaultValue)
{
    qInfo() << Q_FUNC_INFO
            << ", appId: " << appId
            << ", name: " << name
            << ", subpath: " << subpath
            << ", key: " << key
            << ", default value: " << defaultValue;

    DConfig *dConfig = dConfigObject(appId, name, subpath);
    if (!dConfig) {
        qWarning() << "DConfig object is null";
        return defaultValue;
    }

    if (!dConfig->keyList().contains(key))
        return defaultValue;

    const QVariant &value = dConfig->value(key);
    qDebug() << "config value: " << value;
    return value;
}

void DConfigHelper::setConfig(const QString &appId, const QString &name, const QString &subpath, const QString &key, const QVariant &value)
{
    qInfo() << Q_FUNC_INFO
            << ", appId: " << appId
            << ", name: " << name
            << ", subpath: " << subpath
            << ", key: " << key
            << ", value: " << value;

    DConfig *dConfig = dConfigObject(appId, name, subpath);
    if (!dConfig) {
        qWarning() << "DConfig object is null";
        return;
    }

    if (!dConfig->keyList().contains(key)) {
        qWarning() << "DConfig does not contain key: " << key;
        return;
    }

    dConfig->setValue(key, value);
}

DConfig* DConfigHelper::dConfigObject(const QString &appId, const QString &name, const QString &subpath)
{
    const QString &configPath = packageDConfigPath(appId, name, subpath);
    DConfig *dConfig = nullptr;
    if (m_dConfigs.contains(configPath))
        dConfig = m_dConfigs.value(configPath);
    else
        dConfig = initializeDConfig(appId, name, subpath);

    return dConfig;
}

QString DConfigHelper::packageDConfigPath(const QString &appId, const QString &name, const QString &subpath) const
{
    return appId + name + subpath;
}
