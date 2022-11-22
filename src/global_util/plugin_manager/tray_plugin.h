// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYPLUGIN_H
#define TRAYPLUGIN_H

#include "plugin_base.h"
#include "tray_module_interface.h"

class TrayPlugin : public PluginBase
{
    Q_OBJECT
public:
    explicit TrayPlugin(dss::module::TrayModuleInterface *module, QObject *parent = nullptr);

    virtual PluginBase::ModuleType type() const { return PluginBase::ModuleType::TrayType; };

    virtual QString icon() const;

    virtual void setMessageCallback(MessageCallbackFunc);

    virtual void setAppData(AppDataPtr);

    virtual QString message(const QString &);

    virtual QWidget *itemWidget() const;

    virtual QWidget *itemTipsWidget() const;

    virtual const QString itemContextMenu() const;

    virtual void invokedMenuItem(const QString &menuId, const bool checked) const;
};

#endif // TRAYPLUGIN_H
