// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfig_helper.h"

#include "public_func.h"

#include <qdebug.h>

#include <QMetaMethod>

DCORE_USE_NAMESPACE

Q_GLOBAL_STATIC(DConfigHelper, dConfigWatcher)

DConfigHelper::DConfigHelper(QObject *parent)
    : QObject(parent)
{
    moveToThread(qApp->thread());
    initializeDConfig(getDefaultConfigFileName(), getDefaultConfigFileName(), "");
}

DConfigHelper *DConfigHelper::instance()
{
    return dConfigWatcher;
}

DConfig *DConfigHelper::initializeDConfig(const QString &appId,
                                          const QString &name,
                                          const QString &subpath)
{
    QMutexLocker locker(&m_mutex);
    DConfig *dConfig = DConfig::create(appId, name, subpath);
    if (!dConfig) {
        qCWarning(DDE_SHELL) << "Create DConfig failed, appId: " << appId << ", name: " << name
                   << ", subpath: " << subpath;
        return nullptr;
    }

    m_dConfigs[packageDConfigPath(appId, name, subpath)] = dConfig;
    m_bindInfos[dConfig] = {};

    dConfig->moveToThread(qApp->thread());
    dConfig->setParent(this);

    // 即时响应数据变化
    connect(dConfig, &DConfig::valueChanged, this, [this, dConfig](const QString &key) {
        const QVariant &value = dConfig->value(key);
        auto it = m_bindInfos.find(dConfig);
        if (it == m_bindInfos.end())
            return;

        auto itBindInfo = it.value().begin();
        for (; itBindInfo != it.value().end(); ++itBindInfo) {
            if (itBindInfo.value().contains(key)) {
                auto callbackIt = m_objCallbackMap.find(itBindInfo.key());
                if (callbackIt != m_objCallbackMap.end())
                    callbackIt.value()(key, value, itBindInfo.key());
            }
        }
    });

    return dConfig;
}

void DConfigHelper::bind(QObject *obj, const QString &key, OnPropertyChangedCallback callback)
{
    bind(getDefaultConfigFileName(), getDefaultConfigFileName(), "", obj, key, callback);
}

void DConfigHelper::bind(const QString &appId,
                         const QString &name,
                         const QString &subpath,
                         QObject *obj,
                         const QString &key,
                         OnPropertyChangedCallback callback)
{
    if (!obj)
        return;

    DConfig *dConfig = dConfigObject(appId, name, subpath);
    if (!dConfig) {
        qCWarning(DDE_SHELL) << "DConfig is nullptr";
        return;
    }

    auto it = m_bindInfos.find(dConfig);
    if (it == m_bindInfos.end()) {
        qCWarning(DDE_SHELL) << "Can not find bind info";
        return;
    }

    QMap<QObject *, QStringList> &bindInfo = it.value();
    auto bindInfoIt = bindInfo.find(obj);
    if (bindInfoIt != bindInfo.end()) {
        if (!bindInfoIt.value().contains(key))
            bindInfoIt.value().append(key);
    } else {
        bindInfo[obj] = QStringList(key);
    }

    m_objCallbackMap.insert(obj, callback);
    connect(obj, &QObject::destroyed, this, [this, obj] {
        unBind(obj);
    });
}

DConfig *DConfigHelper::defaultDConfigObject()
{
    return dConfigObject(getDefaultConfigFileName(), getDefaultConfigFileName(), "");
}

void DConfigHelper::unBind(QObject *obj, const QString &key)
{
    qCInfo(DDE_SHELL) << "Unbind, obj: " << obj << ", key: " << key;
    if (!obj) {
        qCWarning(DDE_SHELL) << "Unbinding object is null";
        return;
    }

    bool objStillUseful = false;
    auto it = m_bindInfos.begin();
    for (; it != m_bindInfos.end(); ++it) {
        if (key.isEmpty()) {
            it->remove(obj);
        } else {
            // 移除key，移除完如果obj没有绑定了key了，那么把obj也移除掉
            auto it1 = it.value().find(obj);
            if (it1 != it.value().end()) {
                it1.value().removeAll(key);
                if (it1.value().isEmpty()) {
                    it->remove(obj);
                } else {
                    objStillUseful = true;
                }
            }
        }
    }

    if (key.isEmpty() || !objStillUseful)
        m_objCallbackMap.remove(obj);
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

QVariant DConfigHelper::getConfig(const QString &appId,
                                  const QString &name,
                                  const QString &subpath,
                                  const QString &key,
                                  const QVariant &defaultValue)
{
    DConfig *dConfig = dConfigObject(appId, name, subpath);
    if (!dConfig) {
        qCWarning(DDE_SHELL) << "DConfig object is null";
        return defaultValue;
    }

    if (!dConfig->keyList().contains(key))
        return defaultValue;

    const QVariant &value = dConfig->value(key);
    return value;
}

void DConfigHelper::setConfig(const QString &appId,
                              const QString &name,
                              const QString &subpath,
                              const QString &key,
                              const QVariant &value)
{
    DConfig *dConfig = dConfigObject(appId, name, subpath);
    if (!dConfig) {
        qCWarning(DDE_SHELL) << "DConfig object is null";
        return;
    }

    if (!dConfig->keyList().contains(key)) {
        qCWarning(DDE_SHELL) << "DConfig does not contain key: " << key;
        return;
    }

    dConfig->setValue(key, value);
}

DConfig *DConfigHelper::dConfigObject(const QString &appId,
                                      const QString &name,
                                      const QString &subpath)
{
    const QString &configPath = packageDConfigPath(appId, name, subpath);
    DConfig *dConfig = nullptr;
    if (m_dConfigs.contains(configPath))
        dConfig = m_dConfigs.value(configPath);
    else
        dConfig = initializeDConfig(appId, name, subpath);

    return dConfig;
}

QString DConfigHelper::packageDConfigPath(const QString &appId,
                                          const QString &name,
                                          const QString &subpath) const
{
    return appId + name + subpath;
}
