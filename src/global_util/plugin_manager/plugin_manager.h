// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "base_module_interface.h"
#include "login_plugin.h"
#include "tray_plugin.h"
#include "plugin_base.h"

#include <QObject>
#include <QJsonObject>

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    static PluginManager* instance();

    void addPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    LoginPlugin *getLoginPlugin() const;
    QList<TrayPlugin*> trayPlugins() const;
    bool contains(const QString &key) const;
    PluginBase *findPlugin(const QString &key) const;

signals:
    void trayPluginAdded(TrayPlugin *);

public slots:

private:
    PluginBase* createPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    LoginPlugin *createLoginPlugin(dss::module::BaseModuleInterface *module, const QString &version);
    TrayPlugin *createTrayPlugin(dss::module::BaseModuleInterface *module, const QString &version);

private:
    QMap<QString, PluginBase*> m_plugins;
};

#endif // PLUGIN_MANAGER_H