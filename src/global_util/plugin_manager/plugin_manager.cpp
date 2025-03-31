// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_manager.h"
#include "constants.h"
#include "dconfig_helper.h"
#include "login_plugin_v1.h"
#include "login_plugin_v2.h"
#include "public_func.h"
#include "modules_loader.h"

static PluginManager pluginManager;

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
    // 有些插件需要通过 MessageCallback 获取信息来判断自己是否要被启用，但是 CustomAuth 需要判断为被启用后才会被创建，这里有冲突
    // TODO MessageCallback 应该走 PluginManager，而不是CustomAuth
    connect(&ModulesLoader::instance(), &ModulesLoader::loadPluginsFinished, this, &PluginManager::broadcastCurrentUser);
}

PluginManager* PluginManager::instance()
{
    return &pluginManager;
}

void PluginManager::addPlugin(dss::module::BaseModuleInterface* module, const QString& version)
{
    if (!module) {
        qCWarning(DDE_SHELL) << "Module is null.";
        return;
    }

    qCInfo(DDE_SHELL) << "Add plugin:" << module->key() << ", version:" << version;
    PluginBase* plugin = createPlugin(module, version);
    if (!plugin) {
        qCWarning(DDE_SHELL) << "Create plugin failed.";
        return;
    }

    const QString& key = plugin->key();
    m_plugins.insert(key, plugin);
    connect(plugin, &QObject::destroyed, this, [this, key] {
        qCInfo(DDE_SHELL) << key << " was destroyed.";
        m_plugins.remove(key);
    });
}

void PluginManager::removePlugin(const QString &key)
{
    qCInfo(DDE_SHELL) << "Remove plugin:" << key;
    auto it = m_plugins.find(key);
    if (it == m_plugins.end()) {
        qCWarning(DDE_SHELL) << "Plugin not found:" << key;
        return;
    }
    Q_EMIT pluginAboutToBeRemoved(key);
    if (it.value())
        it.value()->deleteLater();
    m_plugins.remove(key);
}


/**
 * @brief 获取登录插件
 *
 * @param level 登录插件级别，1 为普通插件，2 为域管二级插件
 * @return QList<LoginPlugin*>
 */
QList<LoginPlugin*> PluginManager::getLoginPlugins(int level) const
{
    // 指定插件 >> 默认插件
    const auto& designatedLoginPlugin = DConfigHelper::instance()->getConfig("designatedLoginPlugin", "").toString();
    const auto& defaultLoginPlugin = DConfigHelper::instance()->getConfig("defaultLoginPlugin", "").toString();
    auto designatedLoginPlugins = DConfigHelper::instance()->getConfig("designatedLoginPlugins", QStringList()).toStringList();
    auto defaultLoginPlugins = DConfigHelper::instance()->getConfig("defaultLoginPlugins", QStringList()).toStringList();
    qCInfo(DDE_SHELL) << "Designated login plugin:" << designatedLoginPlugin
        << "Default login plugins" << defaultLoginPlugin
        << "Designated login plugins:" << designatedLoginPlugins
        << "Default login plugins:" << defaultLoginPlugins;

    if (!designatedLoginPlugin.isEmpty())
        designatedLoginPlugins.append(designatedLoginPlugin);
    if (!defaultLoginPlugin.isEmpty())
        defaultLoginPlugins.append(defaultLoginPlugin);
    const auto& finalPlugins = !designatedLoginPlugins.isEmpty() ? designatedLoginPlugins : defaultLoginPlugins;
    qCInfo(DDE_SHELL) << "Final login plugins:" << finalPlugins;
    qCInfo(DDE_SHELL) << "All plugins:" << m_plugins.keys();
    QMap<QString, LoginPlugin*> map;
    for (const auto& plugin : m_plugins.values()) {
        if (plugin && PluginBase::ModuleType::LoginType == plugin->type()) {
            if (!finalPlugins.isEmpty() && !finalPlugins.contains(plugin->key()))
                continue;

            auto authPlugin = dynamic_cast<LoginPlugin*>(plugin);
            if (authPlugin && authPlugin->level() == level)
                map.insert(authPlugin->key(), authPlugin);
        }
    }

    // 根据配置排序
    QList<LoginPlugin*> list;
    const auto& order = DConfigHelper::instance()->getConfig(DDESESSIONCC::LOGIN_PLUGIN_DISPLAY_ORDER, {}).toStringList();
    qCInfo(DDE_SHELL) << "Login plugin display order:" << order;
    for (const auto& key : order) {
        const auto plugin = map.take(key);
        if (plugin)
            list.append(plugin);
    }
    list.append(map.values());
    return list;
}

LoginPlugin* PluginManager::getFullManagedLoginPlugin() const
{
    for (const auto& plugin : m_plugins.values()) {
        if (plugin && PluginBase::ModuleType::FullManagedLoginType == plugin->type()) {
            return dynamic_cast<LoginPlugin*>(plugin);
        }
    }

    return nullptr;
}

QList<TrayPlugin*> PluginManager::trayPlugins() const
{
    QList<TrayPlugin*> list;
    for (const auto& plugin : m_plugins.values()) {
        if (plugin && PluginBase::ModuleType::TrayType == plugin->type()) {
            list.append(dynamic_cast<TrayPlugin*>(plugin));
        }
    }

    return list;
}

PluginBase* PluginManager::createPlugin(dss::module::BaseModuleInterface* module, const QString& version)
{
    if (dss::module::BaseModuleInterface::LoginType == module->type()
        || dss::module::BaseModuleInterface::FullManagedLoginType == module->type()
        || dss::module::BaseModuleInterface::IpcAssistLoginType == module->type()) {
        return createLoginPlugin(module, version);
    } else if (dss::module::BaseModuleInterface::TrayType == module->type()) {
        TrayPlugin* plugin = createTrayPlugin(module, version);
        trayPluginAdded(plugin);
        return plugin;
    }

    qCWarning(DDE_SHELL) << "This plug-in type is not recognized: " << module->type();
    return nullptr;
}

LoginPlugin* PluginManager::createLoginPlugin(dss::module::BaseModuleInterface* module, const QString& version)
{
    qCInfo(DDE_SHELL) << "Create login plugin, meta version: " << version;
    if (checkVersion(version, LoginPlugin_V2::API_VERSION)) {
        qCInfo(DDE_SHELL) << "Create LoginPluginV2";
        return new LoginPlugin_V2::LoginPluginV2(dynamic_cast<dss::module_v2::LoginModuleInterfaceV2*>(module));
    } else if (checkVersion(version, LoginPlugin_V1::API_VERSION)) {
        qCInfo(DDE_SHELL) << "Create LoginPluginV1";
        return new LoginPlugin_V1::LoginPluginV1(dynamic_cast<dss::module::LoginModuleInterface*>(module));
    }

    return nullptr;
}

TrayPlugin* PluginManager::createTrayPlugin(dss::module::BaseModuleInterface* module, const QString& version)
{
    Q_UNUSED(version)

    return new TrayPlugin(dynamic_cast<dss::module::TrayModuleInterface*>(module));
}

bool PluginManager::contains(const QString& key) const
{
    return m_plugins.contains(key);
}

PluginBase* PluginManager::findPlugin(const QString& key) const
{
    if (!m_plugins.contains(key))
        return nullptr;

    return m_plugins.value(key);
}

LoginPlugin* PluginManager::getAssistloginPlugin() const
{
    for (const auto& plugin : m_plugins.values()) {
        if (plugin && PluginBase::ModuleType::IpcAssistLoginType == plugin->type()) {
            return dynamic_cast<LoginPlugin*>(plugin);
        }
    }

    return nullptr;
}

void PluginManager::setModel(SessionBaseModel *model)
{
    if (!model)
        return;

    m_model = model;

    connect(m_model, &SessionBaseModel::currentUserChanged, this, &PluginManager::broadcastCurrentUser);
    broadcastCurrentUser();
}

void PluginManager::broadcastCurrentUser()
{
    if (!m_model)
        return;

    const auto &currentUser = m_model->currentUser();
    if (!currentUser)
        return;

    for (const auto &plugin : m_plugins.values()) {
        auto loginPlugin = dynamic_cast<LoginPlugin*>(plugin);
        if (!loginPlugin)
            continue;

        loginPlugin->notifyCurrentUserChanged(currentUser->name(), currentUser->uid());
    }
}

void PluginManager::broadcastAuthFactors(int authFactors)
{
    for (const auto &plugin : m_plugins.values()) {
        auto loginPlugin = dynamic_cast<LoginPlugin*>(plugin);
        if (!loginPlugin)
            continue;

        loginPlugin->notifyAuthFactorsChanged(authFactors);
    }
}
