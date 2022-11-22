// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_plugin.h"
#include "public_func.h"


LoginPlugin::LoginPlugin(dss::module::BaseModuleInterface *module, QObject *parent)
    : PluginBase(module, parent)
    , m_authType(AuthType::AT_Password)
{

}

bool LoginPlugin::isPluginEnabled()
{
    qDebug() << Q_FUNC_INFO;
    QJsonObject message;
    message["CmdType"] = "IsPluginEnabled";
    const QString &result = this->message(toJson(message));
    qDebug() << "Result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty() || !dataObj.contains("IsPluginEnabled"))
        return true;

    return dataObj["IsPluginEnabled"].toBool(true);
}

bool LoginPlugin::supportDefaultUser()
{
    qDebug() << Q_FUNC_INFO;
    QJsonObject message;
    message["CmdType"] = "IsPluginEnabled";
    const QString &result = this->message(toJson(message));
    qDebug() << "Result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty() || !dataObj.contains("IsPluginEnabled"))
        return true;

    return dataObj["IsPluginEnabled"].toBool(true);
}

void LoginPlugin::notifyCurrentUserChanged(const QString &userName)
{
    QJsonObject message;
    message["CmdType"] = "CurrentUserChanged";
    QJsonObject user;
    user["Name"] = userName;
    message["Data"] = user;

    this->message(toJson(message));
}

void LoginPlugin::updateConfig()
{
    QJsonObject message;
    message["CmdType"] = "GetConfigs";
    QString result = this->message(toJson(message));
    qInfo() << "Plugin result: " << result;

    QJsonParseError jsonParseError;
    const QJsonDocument resultDoc = QJsonDocument::fromJson(result.toLatin1(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || resultDoc.isEmpty()) {
        qWarning() << "Result json parse error";
        return;
    }

    QJsonObject resultObj = resultDoc.object();
    if (!resultObj.contains("Data")) {
        qWarning() << "Result does't contains the 'data' field";
        return;
    }

    QJsonObject dataObj = resultObj["Data"].toObject();
    m_pluginConfig.showAvatar = dataObj["ShowAvatar"].toBool(m_pluginConfig.showAvatar);
    m_pluginConfig.showUserName = dataObj["ShowUserName"].toBool(m_pluginConfig.showUserName);
    m_pluginConfig.showSwitchButton = dataObj["ShowSwitchButton"].toBool(m_pluginConfig.showSwitchButton);
    m_pluginConfig.showLockButton = dataObj["ShowLockButton"].toBool(m_pluginConfig.showLockButton);
    m_pluginConfig.defaultAuthLevel = (DefaultAuthLevel)dataObj["DefaultAuthLevel"].toInt();
    m_authType = (AuthType)dataObj["AuthType"].toInt();
}
