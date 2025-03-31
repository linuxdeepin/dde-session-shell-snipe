// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DConfig>

#include <QMap>
#include <QMutex>
#include <QObject>

using OnPropertyChangedCallback = void (*)(const QString &, const QVariant &, QObject *);

/**
 * @brief 配置变化回调函数
 *
 * @param key 配置键值
 * @param value 配置值
 */
using OnPropertiesChanged = void (*)(const QString &key, const QVariant &value);

class DConfigHelper : public QObject
{
    Q_OBJECT
public:
    explicit DConfigHelper(QObject *parent = nullptr);
    static DConfigHelper *instance();

    // 注意：多个配置文件区分同名的key
    void bind(QObject *obj, const QString &key, OnPropertyChangedCallback callback);
    void bind(const QString &appId,
              const QString &name,
              const QString &subpath,
              QObject *obj,
              const QString &key,
              OnPropertyChangedCallback callback);
    void unBind(QObject *obj, const QString &key = "");

    QVariant getConfig(const QString &appId,
                       const QString &name,
                       const QString &subpath,
                       const QString &key,
                       const QVariant &defaultValue);
    QVariant getConfig(const QString &key, const QVariant &defaultValue);

    void setConfig(const QString &appId,
                   const QString &name,
                   const QString &subpath,
                   const QString &key,
                   const QVariant &value);
    void setConfig(const QString &key, const QVariant &value);

private:
    Q_DISABLE_COPY(DConfigHelper)

    Dtk::Core::DConfig *initializeDConfig(const QString &appId,
                                          const QString &name,
                                          const QString &subpath);
    Dtk::Core::DConfig *dConfigObject(const QString &appId,
                                      const QString &name,
                                      const QString &subpath);
    Dtk::Core::DConfig *defaultDConfigObject();
    QString packageDConfigPath(const QString &appId,
                               const QString &name,
                               const QString &subpath) const;

private:
    QMutex m_mutex;
    QMap<QString, Dtk::Core::DConfig *> m_dConfigs;
    QMap<Dtk::Core::DConfig *, QMap<QObject *, QStringList>> m_bindInfos;
    QMap<QObject *, OnPropertyChangedCallback> m_objCallbackMap;
};
