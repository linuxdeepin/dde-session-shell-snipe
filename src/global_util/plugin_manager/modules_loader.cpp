// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modules_loader.h"
#include "base_module_interface.h"
#include "dconfig_helper.h"
#include "plugin_manager.h"
#include "public_func.h"

#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QMutexLocker>
#include <QDBusInterface>

#include <unistd.h>

const QString ModulesDir = QString("%1/dde-session-shell/modules").arg(CMAKE_INSTALL_FULL_LIBDIR);
const QString ModulesConfigDir = QString("%1/dde-session-shell/modules/config.d/").arg(CMAKE_INSTALL_FULL_LIBDIR);
const QString LOWEST_VERSION = "1.1.0";
const QString LoginType = "Login";
const QString TrayType = "Tray";

ModulesLoader::ModulesLoader(QObject* parent)
    : QThread(parent)
    , m_loadLoginModule(false)
    , m_model(nullptr)
{
}

ModulesLoader::~ModulesLoader()
{
    quit();
    wait();
    
    QMutexLocker locker(&m_mutex);
    for (auto &loader : m_pluginLoaders) {
        cleanupPluginLoader(loader);
    }
    m_pluginLoaders.clear();
}

ModulesLoader& ModulesLoader::instance()
{
    static ModulesLoader modulesLoader;
    return modulesLoader;
}

void ModulesLoader::run()
{
    findModule(ModulesDir);
}

void ModulesLoader::findModule(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        qCDebug(DDE_SHELL) << "Find module, Path: " << path << "is not exists.";
        return;
    }

    auto blackList = DConfigHelper::instance()->getConfig("pluginBlackList", QStringList()).toStringList();
    qCInfo(DDE_SHELL) << "Find module, plugin black list:" << blackList;
    const QFileInfoList modules = dir.entryInfoList();
    for (QFileInfo module : modules) {
        const QString path = module.absoluteFilePath();
        if (!QLibrary::isLibrary(path)) {
            continue;
        }

        // 检查是否要加载插件
        if (!isPluginEnabled(module)) {
            qCInfo(DDE_SHELL) << "The plugin dose not want to be loaded, path:" << path;
            // Unload 后再次加载插件会导致插件不显示，且 unload不会降低内存，意义不大
            // QMetaObject::invokeMethod(this, "unloadPlugin", Qt::QueuedConnection, Q_ARG(QString, path));
            continue;
        }

        // 避免重复加载
        if (contains(path)) {
            qCInfo(DDE_SHELL) << "The plugin has been loaded, path:" << path;
            continue;
        }

        qCInfo(DDE_SHELL) << "About to process " << module;
        auto loader = std::unique_ptr<QPluginLoader>(new QPluginLoader(path));

        // 检查兼容性
        const QJsonObject meta = loader->metaData().value("MetaData").toObject();
        const QString version = meta.value("api").toString();
        // 版本过低则不加载，可能会导致登录器崩溃
        if (!checkVersion(version, LOWEST_VERSION)) {
            qCWarning(DDE_SHELL) << "The module version is too low, version:" << version << ", lowest version:" << LOWEST_VERSION;
            continue;
        }

        // 性能优化，分类加载
        QString pluginType = meta.value("pluginType").toString();
        if (!pluginType.isEmpty()) {
            if ((pluginType == LoginType && !m_loadLoginModule) || (pluginType == TrayType && m_loadLoginModule)) {
                continue;
            }
        } else {
            // （兼容）没有pluginType的插件在第一次加载插件的时候就加载
            qCInfo(DDE_SHELL) << "Old plugin has no pluginType in json file:" << module;
        }

        auto* moduleInstance = dynamic_cast<dss::module::BaseModuleInterface*>(loader->instance());
        if (!moduleInstance) {
            qCWarning(DDE_SHELL) << "Load plugin failed, error:" << loader->errorString();
            loader->unload();
            continue;
        }

        qCInfo(DDE_SHELL) << "Current plugin key:" << moduleInstance->key();
        if (blackList.contains(moduleInstance->key())) {
            qCInfo(DDE_SHELL) << "The plugin is in black list, won't be loaded.";
            loader->unload();
            continue;
        }

        int loadPluginType = moduleInstance->loadPluginType();
        if (loadPluginType != dss::module::BaseModuleInterface::Load) {
            qCInfo(DDE_SHELL) << "The plugin dose not want to be loaded.";
            loader->unload();
            continue;
        }

        if (PluginManager::instance()->contains(moduleInstance->key())) {
            qCInfo(DDE_SHELL) << "The plugin has been loaded.";
            loader->unload();
            continue;
        }

        QObject* obj = dynamic_cast<QObject*>(moduleInstance);
        if (obj)
            obj->moveToThread(qApp->thread());
        if (loader)
            loader->moveToThread(qApp->thread());

        QMutexLocker locker(&m_mutex);
        m_pluginLoaders.insert(moduleInstance->key(), QPointer<QPluginLoader>(loader.get()));
        loader.release(); // 释放所有权，防止 QPluginLoader 被析构
        PluginManager::instance()->addPlugin(moduleInstance, version);
    }
    m_loadLoginModule = true;
    Q_EMIT loadPluginsFinished();
}

/**
 * @brief 获取插件是否需要被加载，在插件被 load 之前就可以判断，默认返回 true
 * 从 /usr/lib/dde-session-shell/modules/config.d 文件夹中获取和插件同名（去掉lib后）的 json 文件，从 json 文件中获取 dconfig 的配置信息
 * json 文件示例：
 * "pluginEnabled": {
 *       "lock": {
 *            "dbusProperty": {
 *                "service": "com.deepin.daemon.Accounts",
 *                "path": "/com/deepin/daemon/Accounts/User%{uid}",
 *                "interface": "com.deepin.daemon.Accounts.User",
 *                "dbusConnection": "systembus",
 *                "property": "WeChatAuthEnabled"
 *            }
 *        },
 *        "greeter": {
 *            "shell": "/usr/bin/check-wechat-auth-enabled.sh"
 *        },
 * }
 * `lock`字段表明这个配置适用于锁屏界面；`greeter`字段表明这个配置适用于登录界面
 * `dbusProperty`字段表明需要通过 dbus 获取属性，并且可以监听属性变化；`shell`字段表明是一个 shell 命令，执行命令获取返回值，0 是加载，其它不加载。
 *
 * 另外还支持 dconfig 类型，例如：
 *
 * {
 *   "pluginEnabled": {
 *      "lock": {
 *          "dconfig": {
 *                "appid": "org.deepin.dde.lock",
 *                "resource": "org.deepin.dde.dss-qrcode-auth-plugin",
 *                "subpath": "",
 *                "key": "qrcodeAuthEnabled"
 *            }
 *      }
 * }
 * `qrcodeAuthEnabled`是一个 bool 类型的 dconfig，程序监听 qrcodeAuthEnabled 配置的变化，动态加载/卸载插件

 *
 * @param module 插件文件信息
 * @return true 需要被加载
 * @return false 不需要被加载
 */
bool ModulesLoader::isPluginEnabled(const QFileInfo& module)
{
    QString configFileName = module.baseName();
    if (configFileName.startsWith("lib")) {
        configFileName = configFileName.right(configFileName.length() - 3);
    }
    const QString configFile = ModulesConfigDir + configFileName + ".json";
    if (!QFile::exists(configFile))
        return true;

    qCInfo(DDE_SHELL) << "Find module config file:" << configFile;
    QFile f(configFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(DDE_SHELL) << "Open module config file failed.";
        return true;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    f.close();
    if (doc.isNull() || parseError.error != QJsonParseError::NoError) {
        qCWarning(DDE_SHELL) << "Parse module config file failed, error:" << parseError.errorString();
        return true;
    }

    // 执行到下面的阶段，那么认定插件是必须要通过判断后才能加载的，一旦出现异常则不加载插件
    const auto& obj = doc.object();
    if (obj.contains("pluginEnabled")) {
        const auto& enabledObj = obj.value("pluginEnabled").toObject();
        if (enabledObj.isEmpty()) {
            qCWarning(DDE_SHELL) << "Check load state is not valid.";
            return false;
        }
        qCDebug(DDE_SHELL) << "Plugin enabled object:" << enabledObj;
        const auto appType = qApp->property("dssAppType").toInt();
        QString configField = appType == APP_TYPE_LOCK ? "lock" : "greeter";
        qCDebug(DDE_SHELL) << "configField:" << configField;
        if (!enabledObj.contains(configField)) {
            configField = "all";
            if (!enabledObj.contains(configField)) {
                qCWarning(DDE_SHELL) << "Do not exist valid field, can not get plugin enabled state.";
                return false;
            }
        }
        const auto &detailObj = enabledObj.value(configField).toObject();
        if (detailObj.contains("dconfig")) {
            // 通过 dconfig 检查插件是否可以加载
            const auto &dconfigObj = detailObj.value("dconfig").toObject();
            const auto& appid = dconfigObj.value("appid").toString();
            const auto& resource = dconfigObj.value("resource").toString();
            const auto& subpath = dconfigObj.value("subpath").toString();
            const auto& key = dconfigObj.value("key").toString();
            qCInfo(DDE_SHELL) << "Check load state appid:" << appid << ", resource:" << resource << ", subpath:" << subpath << ", key:" << key;
            if (appid.isEmpty() || resource.isEmpty() || key.isEmpty()) {
                qCWarning(DDE_SHELL) << "Check load state is not valid.";
                return false;
            }
            if (appType == APP_TYPE_LOCK) {
                DConfigHelper::instance()->bind(appid, resource, subpath, this, key, &ModulesLoader::onDConfigPropertyChanged);
            }
            const bool pluginEnabled = DConfigHelper::instance()->getConfig(appid, resource, subpath, key, true).toBool();
            qCInfo(DDE_SHELL) << "Plugin is enabled:" << pluginEnabled;
            return pluginEnabled;
        } else if (detailObj.contains("dbusProperty")){
            // 通过 dbusProperty 检查插件是否可以加载
            const auto &dbusPropertyObj = detailObj.value("dbusProperty").toObject();
            const auto& service = dbusPropertyObj.value("service").toString();
            const auto& path = dbusPropertyObj.value("path").toString();
            const auto& interface = dbusPropertyObj.value("interface").toString();
            const auto& property = dbusPropertyObj.value("property").toString();
            qCInfo(DDE_SHELL) << "Check plugin enabled service:" << service << ", path:" << path << ", interface:" << interface << ", property:" << property;
            if (service.isEmpty() || path.isEmpty() || interface.isEmpty() || property.isEmpty()) {
                qCWarning(DDE_SHELL) << "Check plugin enabled service is not valid.";
                return false;
            }
            QString targetPath = path;

            // 目前仅在 lock 中需要做这个处理
            if (targetPath.contains("%{uid}") && appType == APP_TYPE_LOCK) {
                targetPath = targetPath.replace("%{uid}", QString::number(static_cast<int>(getuid())));
                qCInfo(DDE_SHELL) << "Check plugin enabled targetPath:" << targetPath;
            }
            auto dbusConnection = dbusPropertyObj.value("dbusConnection").toString() == "systembus" ? QDBusConnection::systemBus() : QDBusConnection::sessionBus();
            auto dbusInterface = new QDBusInterface(service, targetPath, interface, dbusConnection);
            if (!dbusInterface || !dbusInterface->isValid()) {
                qCWarning(DDE_SHELL) << "Check plugin enabled dbus interface is not valid.";
                dbusInterface->deleteLater();
                return false;
            }
            const bool pluginEnabled = dbusInterface->property(property.toStdString().c_str()).toBool();
            dbusInterface->deleteLater();
            qCInfo(DDE_SHELL) << "Plugin is enabled:" << pluginEnabled;
            // 监听属性变化
            if (appType == APP_TYPE_LOCK) {
                dbusConnection.connect(
                    service,
                    targetPath,
                    "org.freedesktop.DBus.Properties",
                    "PropertiesChanged",
                    this,
                    SLOT(onDbusPropertiesChanged(QString, QVariantMap, QStringList)));
                m_dbusInfo.insert(interface, property);
            }
            return pluginEnabled;

        } else if (detailObj.contains("shell")) {
            const auto &cmd = detailObj.value("shell").toString();
            qCInfo(DDE_SHELL) << "Check plugin enabled shell:" << cmd;
            if (cmd.isEmpty()) {
                qCWarning(DDE_SHELL) << "Check plugin enabled shell is empty";
                return false;
            }
            QProcess process;
            process.start(cmd);
            if (!process.waitForFinished()) {
                qCWarning(DDE_SHELL) << "Execute shell timeout";
                return false;
            }
            const int exitCode = process.exitCode();
            qCInfo(DDE_SHELL) << "Execute shell ret:" << exitCode;
            return exitCode == 0;
        }
    }
    return true;
}

bool ModulesLoader::contains(const QString& pluginFile) const
{
    QMutexLocker locker(&m_mutex);
    for (const auto& loader : m_pluginLoaders.values()) {
        if (loader && loader->fileName() == pluginFile) {
            return true;
        }
    }
    return false;
}

QPair<QString, QPluginLoader*> ModulesLoader::getPluginLoader(const QString& pluginFile) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_pluginLoaders.begin();
    for (; it != m_pluginLoaders.end(); it++) {
        const auto loader = it.value();
        if (loader && loader->fileName() == pluginFile) {
            return qMakePair(it.key(), loader);
        }
    }
    return qMakePair(QString(), nullptr);
}

void ModulesLoader::unloadPlugin(const QString& path)
{
    if (contains(path)) {
        auto pair = getPluginLoader(path);
        if (!pair.first.isEmpty() && pair.second) {
            qCInfo(DDE_SHELL) << "Remove plugin: " << pair.first;
            PluginManager::instance()->removePlugin(pair.first);
            QMutexLocker locker(&m_mutex);
            m_pluginLoaders.remove(pair.first);
            qCInfo(DDE_SHELL) << "Unload plugin loader";
            pair.second->unload();
        }
    }
}

void ModulesLoader::cleanupPluginLoader(QPluginLoader* loader)
{
    if (loader) {
        loader->unload();
        loader->deleteLater();
    }
}

// TODO 只重新加载/卸载变更的插件
void ModulesLoader::onDConfigPropertyChanged(const QString& key, const QVariant& value, QObject* objPtr)
{
    auto obj = qobject_cast<ModulesLoader*>(objPtr);
    if (!obj)
        return;

    qCInfo(DDE_SHELL) << "DConfig property changed, key: " << key << ", value: " << value;
    obj->start(QThread::LowestPriority);
}

void ModulesLoader::onDbusPropertiesChanged(const QString& interfaceName, const QVariantMap& changedProperties, const QStringList& invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties)
    qCDebug(DDE_SHELL) << "Dbus properties changed, interface: " << interfaceName << ", changed properties: " << changedProperties;
    if (m_dbusInfo.contains(interfaceName)) {
        const auto &property = m_dbusInfo.value(interfaceName);
        if (changedProperties.contains(property)) {
            start(QThread::LowestPriority);
        }
    }
}
