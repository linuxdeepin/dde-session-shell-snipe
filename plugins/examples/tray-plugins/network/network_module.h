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

#ifndef LOGIN_MODULE_H
#define LOGIN_MODULE_H

#include "tray_module_interface.h"

class QLabel;

namespace dss {
namespace module {

class NetworkModule : public QObject
    , public TrayModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Tray" FILE "network.json")
    Q_INTERFACES(dss::module::TrayModuleInterface)

public:
    explicit NetworkModule(QObject *parent = nullptr);
    ~NetworkModule() override;

    void init() override;

    inline QString key() const override { return objectName(); }
    QWidget *content() override { return m_networkWidget; }
    virtual inline QString icon() const override { return ":/img/default_avatar.svg"; }

    virtual QWidget *itemWidget() const override;
    virtual QWidget *itemTipsWidget() const override;
    virtual const QString itemContextMenu() const override;
    virtual void invokedMenuItem(const QString &menuId, const bool checked) const override;

private:
    void initUI();

private:
    QWidget *m_networkWidget;
    QLabel *m_tipLabel;
    QLabel *m_itemLabel;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
