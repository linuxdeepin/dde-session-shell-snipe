// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLUGIN_CONFIG_MAP_H
#define PLUGIN_CONFIG_MAP_H

#include <QObject>
#include <QMap>

// retirve some properties for lockcontent from plugins
class PluginConfig
{
public:
    PluginConfig() {}
    virtual ~PluginConfig() {}

    // if false, switch user button would be hidden in base session window
    // override if nesscessary
    // defalut return true
    // you should get correct config from plugin
    virtual bool isUserSwitchButtonVisiable() const { return true; }
};

class PluginConfigMap
{
public:
    enum ConfigIndex {
        FullManagePlugin,
        Max = FullManagePlugin
    };

    ~PluginConfigMap() {}

    static PluginConfigMap &instance();
    const PluginConfig *getConfig();

public slots:
    void requestAddConfig(ConfigIndex index, const PluginConfig *);
    void requestRemoveConfig(ConfigIndex);

private:
    QMap<ConfigIndex, const PluginConfig *> m_configs;
    PluginConfigMap() {}
    PluginConfigMap(const PluginConfigMap &);
    PluginConfigMap &operator=(const PluginConfigMap &);
};

#endif // PLUGIN_CONFIG_MAP_H
