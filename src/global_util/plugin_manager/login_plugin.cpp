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
    QJsonObject message;
    message["CmdType"] = "IsPluginEnabled";

    const QString &result = this->message(toJson(message));
    qCDebug(DDE_SHELL) << key() << " get enabled state result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty() || !dataObj.contains("IsPluginEnabled"))
        return true;

    return dataObj["IsPluginEnabled"].toBool(true);
}

int LoginPlugin::level()
{
    QJsonObject message {
        {"CmdType", "GetLevel"}
    };

    const QString &result = this->message(toJson(message));
    const QJsonObject &dataObj = getDataObj(result);
    if (!dataObj.contains("Level")) {
        return 1;
    }

    return dataObj["Level"].toInt(1);
}

int LoginPlugin::loginType()
{
    QJsonObject message {
        {"CmdType", "GetLoginType"}
    };

    const QString &result = this->message(toJson(message));
    qCDebug(DDE_SHELL) << "Login type, result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (!dataObj.contains("LoginType")) {
        return CustomLoginType::CLT_Default;
    }

    return dataObj["LoginType"].toInt(CustomLoginType::CLT_Default);
}

bool LoginPlugin::hasSecondLevel(const QString &user)
{
    QJsonObject message {
        {"CmdType", "HasSecondLevel"},
        {"Data", user}
    };

    const QString &result = this->message(toJson(message));
    const QJsonObject &dataObj = getDataObj(result);
    if (!dataObj.contains("HasSecondLevel")) {
        return false;
    }

    return dataObj["HasSecondLevel"].toBool(false);
}

int LoginPlugin::updateLoginType()
{
    QJsonObject message {
        {"CmdType", "UpdateLoginType"}
    };

    const QString &result = this->message(toJson(message));
    qCDebug(DDE_SHELL) << "Update login type, result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (!dataObj.contains("LoginType")) {
        return CustomLoginType::CLT_Default;
    }

    return dataObj["LoginType"].toInt(CustomLoginType::CLT_Default);
}

int LoginPlugin::sessionTimeout()
{
    QJsonObject message {
        {"CmdType", "GetSessionTimeout"}
    };

    const QString &result = this->message(toJson(message));
    qCDebug(DDE_SHELL) << "Get session timeout, result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (!dataObj.contains("SessionTimeout")) {
        return DEFAULT_SESSION_TIMEOUT;
    }

    return dataObj["SessionTimeout"].toInt(DEFAULT_SESSION_TIMEOUT);
}

bool LoginPlugin::supportDefaultUser()
{
    QJsonObject message;
    message["CmdType"] = "GetConfigs";
    const QString &result = this->message(toJson(message));
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty())
        return true;

    return dataObj["SupportDefaultUser"].toBool(true);
}

void LoginPlugin::notifyCurrentUserChanged(const QString &userName, uid_t uid)
{
    QJsonObject message;
    message["CmdType"] = "CurrentUserChanged";
    QJsonObject user;
    user["Name"] = userName;
    user["Uid"] = static_cast<int>(uid);
    message["Data"] = user;

    this->message(toJson(message));
}

void LoginPlugin::updateConfig()
{
    QJsonObject message;
    message["CmdType"] = "GetConfigs";
    QString result = this->message(toJson(message));
    qCInfo(DDE_SHELL) << "Get configs result: " << result;

    QJsonParseError jsonParseError;
    const QJsonDocument resultDoc = QJsonDocument::fromJson(result.toLatin1(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || resultDoc.isEmpty()) {
        qCWarning(DDE_SHELL) << "Result json parse error, error: " << jsonParseError.error;
        return;
    }

    QJsonObject resultObj = resultDoc.object();
    if (!resultObj.contains("Data")) {
        qCWarning(DDE_SHELL) << "Result doesn't contains the 'data' field";
        return;
    }

    QJsonObject dataObj = resultObj["Data"].toObject();
    m_pluginConfig.showAvatar = dataObj["ShowAvatar"].toBool(m_pluginConfig.showAvatar);
    m_pluginConfig.showUserName = dataObj["ShowUserName"].toBool(m_pluginConfig.showUserName);
    m_pluginConfig.showSwitchButton = dataObj["ShowSwitchButton"].toBool(m_pluginConfig.showSwitchButton);
    m_pluginConfig.showLockButton = dataObj["ShowLockButton"].toBool(m_pluginConfig.showLockButton);
    m_pluginConfig.defaultAuthLevel = (DefaultAuthLevel)dataObj["DefaultAuthLevel"].toInt();
    m_pluginConfig.showBackGroundColor = dataObj["ShowBackGroundColor"].toBool(m_pluginConfig.showBackGroundColor);
    m_pluginConfig.switchUserWhenCheckAccount = dataObj["SwitchUserWhenCheckAccount"].toBool(m_pluginConfig.switchUserWhenCheckAccount);
    m_pluginConfig.notUsedByLoginedUserInGreeter = dataObj["NotUsedByLoginedUserInGreeter"].toBool(m_pluginConfig.notUsedByLoginedUserInGreeter);
    m_pluginConfig.saveLastAuthType = dataObj["SaveLastAuthType"].toBool(m_pluginConfig.saveLastAuthType);
    if (dataObj.contains("AssignAuthType"))
        m_pluginConfig.assignAuthType = static_cast<AuthCommon::AuthType>(dataObj["AssignAuthType"].toInt());
    m_authType = (AuthType)dataObj["AuthType"].toInt();
}

void LoginPlugin::accountError()
{
    QJsonObject jsonObject;
    jsonObject["CmdType"] = "AccountError";
    this->message(toJson(jsonObject));
}

void LoginPlugin::notifyAuthFactorsChanged(int authFactors)
{
    QJsonObject message;
    message["CmdType"] = "AuthFactorsChanged";
    QJsonObject data;
    data["AuthFactors"] = authFactors;
    message["Data"] = data;

    this->message(toJson(message));
}
