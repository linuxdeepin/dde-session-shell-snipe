// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_base.h"

PluginBase::PluginBase(dss::module::BaseModuleInterface* module, QObject *parent)
    : QObject(parent)
    , m_plugin(module)
{

}

void PluginBase::init()
{
    if (!m_plugin)
        return;

    m_plugin->init();
}

QString PluginBase::key() const
{
    if (!m_plugin)
        return "";

    return m_plugin->key();
}

QWidget *PluginBase::content()
{
    if (!m_plugin)
        return nullptr;

    return m_plugin->content();
}

PluginBase::LoadType PluginBase::loadPluginType()
{
    if (!m_plugin)
        return LoadType::Notload;

    return m_plugin->loadPluginType();
}

QJsonObject PluginBase::getRootObj(const QString &jsonStr)
{
    QJsonParseError jsonParseError;
    const QJsonDocument &resultDoc = QJsonDocument::fromJson(jsonStr.toLocal8Bit(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || resultDoc.isEmpty()) {
        qCWarning(DDE_SHELL) << "Result json parse error, error: " << jsonParseError.error;
        return QJsonObject();
    }

    return resultDoc.object();
}

QJsonObject PluginBase::getDataObj(const QString &jsonStr)
{
    const QJsonObject &rootObj = getRootObj(jsonStr);
    if (!rootObj.contains("Data")) {
        qCWarning(DDE_SHELL) << "Result doesn't contains the 'data' field";
        return QJsonObject();
    }

    return rootObj["Data"].toObject();
}
