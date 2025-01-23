// SPDX-FileCopyrightText: 2020 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfigwatcher.h"

#include <QVariant>
#include <QWidget>

DConfigWatcher::DConfigWatcher(QObject *parent)
    : QObject(parent)
    , m_dConfig(Dtk::Core::DConfig::create("org.deepin.dde.session-shell", "org.deepin.dde.session-shell", QString(), this))
{
    connect(m_dConfig, &Dtk::Core::DConfig::valueChanged, this, &DConfigWatcher::onStatusModeChanged);
}

DConfigWatcher *DConfigWatcher::instance()
{
    static DConfigWatcher w;
    return &w;
}

void DConfigWatcher::bind(const QString &key, QWidget *binder)
{
    m_map.insert(key, binder);

    setStatus(key, binder);
}

void DConfigWatcher::erase(const QString &key)
{
    if (m_map.isEmpty() || !m_map.contains(key))
        return;

    m_map.remove(key);
}

void DConfigWatcher::erase(const QString &key, QWidget *binder)
{
    if (m_map.isEmpty() || !m_map.contains(key))
        return;

    m_map.remove(key, binder);
}

void DConfigWatcher::setStatus(const QString &key, QWidget *binder)
{
    if (!binder)
        return;

    const int setting = m_dConfig->value(key).toInt();
    if (setting == Status_Enabled) {
        binder->setEnabled(true);
    } else if (setting == Status_Disabled) {
        binder->setEnabled(false);
    }

    binder->setVisible(setting != Status_Hidden);
}

const int DConfigWatcher::getStatus(const QString &key)
{
    return m_dConfig->value(key).toInt();
}

void DConfigWatcher::onStatusModeChanged(const QString &key)
{
    if (m_map.isEmpty() || !m_map.contains(key))
        return;

    // 重新设置控件对应的显示类型
    for (auto mapUnit = m_map.begin(); mapUnit != m_map.end(); ++mapUnit) {
        if (key == mapUnit.key()) {
            setStatus(key, mapUnit.value());
            emit enableChanged(key, mapUnit.value()->isEnabled());
        }
    }
}
