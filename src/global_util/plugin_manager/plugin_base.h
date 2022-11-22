// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLUGIN_BASE_H
#define PLUGIN_BASE_H

#include "constants.h"
#include "base_module_interface.h"

#include <QObject>
#include <QString>

class PluginBase : public QObject
{
    Q_OBJECT
public:
    using ModuleType = dss::module::BaseModuleInterface::ModuleType;
    using LoadType = dss::module::BaseModuleInterface::LoadType;
    using AppDataPtr = dss::module::AppDataPtr;
    using MessageCallbackFunc = dss::module::MessageCallbackFunc;

    explicit PluginBase(dss::module::BaseModuleInterface* module, QObject *parent = nullptr);

    virtual ModuleType type() const = 0;

    virtual void init();

    virtual QString key() const;

    virtual QString icon() const { return ""; }

    virtual QWidget *content();

    virtual LoadType loadPluginType();

    virtual void setMessageCallback(MessageCallbackFunc func) {}

    virtual void setAppData(void*) {}

    virtual QString message(const QString &) { return ""; }

    static QJsonObject getRootObj(const QString &jsonStr);

    static QJsonObject getDataObj(const QString &jsonStr);

protected:
    dss::module::BaseModuleInterface *m_plugin;
};

#endif // PLUGIN_BASE_H
