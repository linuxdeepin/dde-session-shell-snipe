// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "base_module_interface.h"
#include "login_plugin.h"
#include "tray_plugin.h"
#include "plugin_base.h"
#include "sessionbasemodel.h"

#include <QObject>
#include <QJsonObject>

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    static PluginManager* instance();

    void setModel(SessionBaseModel *model);
    void addPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    void removePlugin(const QString &key);
    QList<LoginPlugin*> getLoginPlugins(int level = 1) const;
    LoginPlugin *getFullManagedLoginPlugin() const;
    LoginPlugin *getAssistloginPlugin() const;
    QList<TrayPlugin*> trayPlugins() const;
    bool contains(const QString &key) const;
    PluginBase *findPlugin(const QString &key) const;
    void broadcastAuthFactors(int authFactors);

signals:
    void trayPluginAdded(TrayPlugin *);
    void pluginAboutToBeRemoved(const QString &key);

private slots:
    void broadcastCurrentUser();

private:
    PluginBase* createPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    LoginPlugin *createLoginPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    TrayPlugin *createTrayPlugin(dss::module::BaseModuleInterface *module, const QString &version);

private:
    QPointer<SessionBaseModel> m_model;
    QMap<QString, PluginBase*> m_plugins;
};

#endif // PLUGIN_MANAGER_H
