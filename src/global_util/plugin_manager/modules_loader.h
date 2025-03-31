// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODULES_LOADER_H
#define MODULES_LOADER_H

#include "sessionbasemodel.h"

#include <QHash>
#include <QThread>
#include <QSharedPointer>
#include <QFileInfo>
#include <QMap>
#include <QPluginLoader>
#include <QPointer>
#include <QMutex>

class ModulesLoader : public QThread
{
    Q_OBJECT
public:
    static ModulesLoader &instance();
    void setLoadLoginModule(bool loadLoginModule) { m_loadLoginModule = loadLoginModule; }
    void setModel(SessionBaseModel *model);

signals:
    void loadPluginsFinished();

protected:
    void run() override;

private:
    explicit ModulesLoader(QObject *parent = nullptr);
    ~ModulesLoader() override;
    ModulesLoader(const ModulesLoader &) = delete;
    ModulesLoader &operator=(const ModulesLoader &) = delete;

    void findModule(const QString &path);
    bool isPluginEnabled(const QFileInfo &module);
    bool contains(const QString &pluginFile) const;
    QPair<QString, QPluginLoader*> getPluginLoader(const QString &pluginFile) const;

    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

private Q_SLOTS:
    void unloadPlugin(const QString &path);
    void onDbusPropertiesChanged(const QString& interfaceName,
        const QVariantMap& changedProperties,
        const QStringList& invalidatedProperties);

private:
    bool m_loadLoginModule;
    QMap<QString, QPointer<QPluginLoader>> m_pluginLoaders;
    QMap<QString, QString> m_dbusInfo;
    mutable QMutex m_mutex;
    QPointer<SessionBaseModel> m_model;
};

#endif // MODULES_LOADER_H
