// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modules_loader.h"

#include "base_module_interface.h"
#include "login_module_interface.h"
#include "tray_module_interface.h"

#include <QDebug>
#include <QDir>
#include <QLibrary>
#include <QPluginLoader>
#include <QGSettings/qgsettings.h>
#include <QApplication>

namespace dss {
namespace module {

const QString ModulesDir = "/usr/lib/dde-session-shell/modules";
const int LoadPlugin = 0;

ModulesLoader::ModulesLoader(QObject *parent)
    : QThread(parent)
{
}

ModulesLoader::~ModulesLoader()
{
}

ModulesLoader &ModulesLoader::instance()
{
    static ModulesLoader modulesLoader;
    return modulesLoader;
}

BaseModuleInterface *ModulesLoader::findModuleByName(const QString &name) const
{
    return m_modules.value(name, nullptr).data();
}

QHash<QString, BaseModuleInterface *> ModulesLoader::findModulesByType(const int type) const
{
    QHash<QString, BaseModuleInterface *> modules;
    for (QSharedPointer<BaseModuleInterface> module : m_modules.values()) {
        if (module.isNull()) {
            continue;
        }

        if (module->type() == type) {
            modules.insert(module->key(), module.data());
        }
    }
    return modules;
}

void ModulesLoader::run()
{
    findModule(ModulesDir);
}

bool ModulesLoader::checkVersion(const QString &target, const QString &base)
{
    if (target == base) {
        return true;
    }
    const QStringList baseVersion = base.split(".");
    const QStringList targetVersion = target.split(".");

    const int baseVersionSize = baseVersion.size();
    const int targetVersionSize = targetVersion.size();
    const int size = baseVersionSize < targetVersionSize ? baseVersionSize : targetVersionSize;

    for (int i = 0; i < size; i++) {
        if (targetVersion[i] == baseVersion[i]) continue;
        return targetVersion[i].toInt() > baseVersion[i].toInt() ? true : false;
    }
    return true;
}

void ModulesLoader::findModule(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << path << "is not exists.";
        return;
    }
    const QFileInfoList modules = dir.entryInfoList();
    for (QFileInfo module : modules) {
        const QString path = module.absoluteFilePath();
        if (!QLibrary::isLibrary(path)) {
            continue;
        }
        qInfo() << module << "is found";
        QPluginLoader loader(path);

        // 检查兼容性
        QString baseVersion = BASE_API_VERSION;
        const QJsonObject &meta = loader.metaData().value("MetaData").toObject();
        if (meta.value("pluginType").toString() == "Login") {
            baseVersion = LOGIN_API_VERSION;
        } else if(meta.value("pluginType").toString() == "Tray") {
            baseVersion = TRAY_API_VERSION;
        }
        // 版本过低则不加载，可能会导致登录器崩溃
        if (!checkVersion(meta.value("api").toString(), baseVersion)) {
            qWarning() << "The module version is too low.";
            continue;
        }

        BaseModuleInterface *moduleInstance = dynamic_cast<BaseModuleInterface *>(loader.instance());
        if (!moduleInstance) {
            qWarning() << loader.errorString();
            continue;
        }

        int loadPluginType = moduleInstance->loadPluginType();
        if (loadPluginType != LoadPlugin) {
            continue;
        }
        if (m_modules.contains(moduleInstance->key())) {
            continue;
        }

        QObject *obj = dynamic_cast<QObject*>(moduleInstance);
        if (obj)
            obj->moveToThread(qApp->thread());

        m_modules.insert(moduleInstance->key(), QSharedPointer<BaseModuleInterface>(moduleInstance));
        emit moduleFound(moduleInstance);
    }
}

void ModulesLoader::removeModule(const QString &moduleKey)
{
    if (!m_modules.contains(moduleKey))
        return;

    m_modules.remove(moduleKey);
}

} // namespace module
} // namespace dss
