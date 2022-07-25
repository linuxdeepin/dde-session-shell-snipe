/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
