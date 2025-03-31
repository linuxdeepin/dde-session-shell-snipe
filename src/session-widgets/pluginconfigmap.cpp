// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pluginconfigmap.h"

PluginConfigMap &PluginConfigMap::instance()
{
    static PluginConfigMap configStack;
    return configStack;
}

const PluginConfig *PluginConfigMap::getConfig()
{
    if (m_configs.size()) {
        // always return the first one
        // config should be unicon, plugins should remove itselfs' config when deactived or unloaded
        return m_configs.begin().value();
    }
    return nullptr;
}

void PluginConfigMap::requestAddConfig(ConfigIndex index, const PluginConfig *config)
{
    if (!config) {
        return;
    }

    m_configs.insert(index, config);
}

void PluginConfigMap::requestRemoveConfig(PluginConfigMap::ConfigIndex index)
{
    if (m_configs.find(index) != m_configs.end()) {
        m_configs.remove(index);
    }
}
