// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assist_login_widget.h"

QList<AssistLoginWidget *> AssistLoginWidget::AssistLoginWidgetObjs = {};

AssistLoginWidget::AssistLoginWidget(QWidget *parent)
    : AuthModule(AuthCommon::AT_Password, parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_module(nullptr)
{
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    AssistLoginWidgetObjs.append(this);
}

AssistLoginWidget::~AssistLoginWidget()
{
    if (m_module && m_module->content() && m_module->content()->parent() == this) {
        m_module->content()->setParent(nullptr);
    }
    m_module->reset();
}

void AssistLoginWidget::setModule(LoginPlugin *module)
{
    m_module = module;

    setCallback();
}

void AssistLoginWidget::initUI()
{
    qCInfo(DDE_SHELL) << Q_FUNC_INFO << "AssistLoginWidget initUi";
    if (!m_module) {
        return;
    }

    if (!m_module->content() || m_module->content()->parent() == nullptr) {
        qCInfo(DDE_SHELL) << Q_FUNC_INFO << "m_module->init()";
        m_module->init();
        m_mainLayout->addWidget(m_module->content());
    }
}

void AssistLoginWidget::setCallback()
{
    if (!m_module) {
        qCWarning(DDE_SHELL) << "Plugin module is null";
        return;
    }
    m_module->setMessageCallback(&AssistLoginWidget::messageCallback);
    m_module->setAppData(this);
    m_module->setAuthCallback(&AssistLoginWidget::authCallback);
}

void AssistLoginWidget::authCallback(const LoginPlugin::AuthCallbackData *callbackData, void *app_data)
{
    qCInfo(DDE_SHELL) << Q_FUNC_INFO << "AuthCustom::authCallback";
    AssistLoginWidget *assistLoginWidget = getAssistLoginWidgetObj(app_data);
    if (callbackData && assistLoginWidget) {
        assistLoginWidget->setAuthData(*callbackData);
    }
}

QString AssistLoginWidget::messageCallback(const QString &message, void *app_data)
{
    qCDebug(DDE_SHELL) << Q_FUNC_INFO << "Received message: " << message;
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(message.toLatin1(), &jsonParseError);

    QJsonObject retObj;
    QJsonObject dataObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        retObj["Code"] = -1;
        retObj["Message"] = "Failed to analysis message info";
        qCWarning(DDE_SHELL) << "Failed to analysis message info from plugin!: " << message;
        return toJson(retObj);
    }

    QJsonObject messageObj = messageDoc.object();
    AssistLoginWidget *assistLoginWidget = getAssistLoginWidgetObj(app_data);
    if (!assistLoginWidget) {
        retObj["Code"] = -1;
        retObj["Message"] = "App data is nullptr!";
        return toJson(retObj);
    }

    QString cmdType = messageObj.value("CmdType").toString();
    qCInfo(DDE_SHELL) << "Cmd type: " << cmdType;
    if (cmdType == "GetProperties") {
    } else if (cmdType == "setAuthTypeInfo") {
    }

    retObj["Data"] = dataObj;
    return toJson(retObj);
}

void AssistLoginWidget::setAuthData(const LoginPlugin::AuthCallbackData &callbackData)
{
    m_currentAuthData = callbackData;
    if (dss::module::AuthResult::Success != callbackData.result) {
        qCWarning(DDE_SHELL) << "Custom auth result is not success";
        return;
    }
    qCDebug(DDE_SHELL) << Q_FUNC_INFO << m_currentAuthData.result;
    const QString &account = callbackData.account;
    const QString &token = callbackData.token;
    if (!account.isEmpty()) {
        qCInfo(DDE_SHELL) << Q_FUNC_INFO << "requestSendToken: " << account << token;
        emit requestSendToken(account, token);
    }
}

void AssistLoginWidget::updateConfig()
{
    if (!m_module) {
        return;
    }

    m_module->updateConfig();
    setPluginConfig(m_module->pluginConfig());
}

AssistLoginWidget *AssistLoginWidget::getAssistLoginWidgetObj(void *app_data)
{
    AssistLoginWidget *assistLoginWidget = static_cast<AssistLoginWidget *>(app_data);
    if (AssistLoginWidgetObjs.contains(assistLoginWidget)) {
        return assistLoginWidget;
    }

    if (AssistLoginWidgetObjs.size() > 0) {
        return AssistLoginWidgetObjs.first();
    }

    return nullptr;
}

QSize AssistLoginWidget::contentSize() const
{
    if (!m_module || !m_module->content()) {
        return QSize();
    }

    return m_module->content()->size();
}

void AssistLoginWidget::startAuth()
{
    qCDebug(DDE_SHELL) << Q_FUNC_INFO << m_currentAuthData.result;
    if (m_currentAuthData.result != 0) {
        setAuthData(m_currentAuthData);
    }
}

void AssistLoginWidget::resetAuth()
{
    qCInfo(DDE_SHELL) << Q_FUNC_INFO << "Reset AssistLoginWidget auth";
    m_currentAuthData = LoginPlugin::AuthCallbackData();
}

const LoginPlugin::PluginConfig &AssistLoginWidget::getPluginConfig() const
{
    return m_pluginConfig;
}

void AssistLoginWidget::setPluginConfig(const LoginPlugin::PluginConfig &pluginConfig)
{
    if (m_pluginConfig != pluginConfig) {
        m_pluginConfig = pluginConfig;
        Q_EMIT requestPluginConfigChanged(m_pluginConfig);
    }
}
