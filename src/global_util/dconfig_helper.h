// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#include <DConfig>
#include <QMap>

/**
 * @brief 配置变化回调函数
 *
 * @param key 配置键值
 * @param value 配置值
 */
using OnPropertiesChanged = void(*) (const QString &key, const QVariant &value);

class DConfigHelper : public QObject
{
    Q_OBJECT
public:
    explicit DConfigHelper(QObject *parent = nullptr);

    static DConfigHelper* instance();

    void bind(QObject *obj, const QString &key);

    /**
     * @brief 如果需要读取其它app的配置文件，使用这个函数进行绑定
     */
    void bind(const QString &appId, const QString &name, const QString &subpath, QObject *obj, const QString *key) {}

    void unBind(QObject *obj, const QString &key = "");

    QVariant getConfig(const QString &appId, const QString &name, const QString &subpath, const QString &key, const QVariant &defaultValue);

    void setConfig(const QString &appId, const QString &name, const QString &subpath, const QString &key, const QVariant &value);

    QVariant getConfig(const QString &key, const QVariant &defaultValue);

    void setConfig(const QString &key, const QVariant &value);

private:
    Dtk::Core::DConfig* initializeDConfig(const QString &appId, const QString &name, const QString &subpath);

    Dtk::Core::DConfig *dConfigObject(const QString &appId, const QString &name, const QString &subpath);

    Dtk::Core::DConfig* defaultDConfigObject();

    QString packageDConfigPath(const QString &appId, const QString &name, const QString &subpath) const;

private:
    QMap<QString, Dtk::Core::DConfig*> m_dConfigs;
    QMap<Dtk::Core::DConfig*, QMap<QString, QList<QObject *>>> m_bindInfos;
};
