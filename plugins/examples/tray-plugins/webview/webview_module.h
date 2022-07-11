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
#include "webview_content.h"

namespace dss {
namespace module {

class WebviewModule : public QObject
    , public TrayModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.deepin.dde.shell.Modules.Tray" FILE "webview.json")
    Q_INTERFACES(dss::module::TrayModuleInterface)

public:
    explicit WebviewModule(QObject *parent = nullptr);
    ~WebviewModule() override;

    void init() override;

    inline QString key() const override { return objectName(); }
    QWidget *content() override { return m_webviewContent; }
    inline QString icon() const override { return ":/img/deepin-system-monitor.svg"; }

private:
    void initUI();

private:
    WebviewContent *m_webviewContent;
};

} // namespace module
} // namespace dss
#endif // LOGIN_MODULE_H
