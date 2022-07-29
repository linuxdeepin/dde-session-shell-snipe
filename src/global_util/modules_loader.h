// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODULES_LOADER_H
#define MODULES_LOADER_H

#include <QHash>
#include <QThread>
#include <QSharedPointer>

namespace dss {
namespace module {

class BaseModuleInterface;
class ModulesLoader : public QThread
{
    Q_OBJECT
public:
    static ModulesLoader &instance();

    inline QHash<QString, QSharedPointer<BaseModuleInterface>> moduleList() { return m_modules; }
    BaseModuleInterface *findModuleByName(const QString &name) const;
    QHash<QString, BaseModuleInterface *> findModulesByType(const int type) const;
    void removeModule(const QString &moduleKey);
    bool moduleExists(const int type) const { return findModulesByType(type).size() > 0; }

signals:
    void moduleFound(BaseModuleInterface *);

protected:
    void run() override;

private:
    explicit ModulesLoader(QObject *parent = nullptr);
    ~ModulesLoader() override;
    ModulesLoader(const ModulesLoader &) = delete;
    ModulesLoader &operator=(const ModulesLoader &) = delete;

    bool checkVersion(const QString &target, const QString &base);
    void findModule(const QString &path);

private:
    QHash<QString, QSharedPointer<BaseModuleInterface>> m_modules;
};

} // namespace module
} // namespace dss
#endif // MODULES_LOADER_H
