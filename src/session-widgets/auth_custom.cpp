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

#include "auth_custom.h"

#include "modules_loader.h"
#include "sessionbasemodel.h"

#include <QBoxLayout>

using namespace dss::module;

AuthCustom::AuthCustom(QWidget *parent)
    : AuthModule(AuthCommon::AT_Custom, parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_module(nullptr)
    , m_model(nullptr)
    , m_showAvatar(true)
    , m_showUserName(true)
    , m_showSwitchButton(true)
    , m_showLockButton(false)
    , m_defaultAuthLevel(AuthCommon::DefaultAuthLevel::Default)
{
    setObjectName(QStringLiteral("AuthCutom"));
    setAccessibleName(QStringLiteral("AuthCutom"));

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
}

AuthCustom::~AuthCustom()
{
    if (m_module && m_module->content() && m_module->content()->parent() == this) {
        m_module->content()->setParent(nullptr);
    }

    if (m_module) {
        delete m_module;
        m_module = nullptr;
    }

}

void AuthCustom::setModule(dss::module::LoginModuleInterface *module)
{
    if (m_module) {
        return;
    }
    m_module = module;

    init();
}

void AuthCustom::setModel(const SessionBaseModel *model)
{
    if (!model || m_model == model)
        return;

    m_model = model;
    // 通知插件当前用户发生变化
    connect(m_model, &SessionBaseModel::currentUserChanged, this, [this] (const std::shared_ptr<User> currentUser) {
        if (!currentUser || !m_module)
            return;

        QJsonObject message;
        message["CmdType"] = "CurrentUserChanged";
        QJsonObject user;
        user["Name"] = currentUser->name();
        message["Data"] = user;

        m_module->onMessage(toJson(message).toStdString());
    });
}

void AuthCustom::init()
{
    m_callback.app_data = this;
    m_callback.authCallbackFun = AuthCustom::authCallBack;
    m_callback.messageCallbackFunc = AuthCustom::messageCallback;
    m_module->setCallback(&m_callback);
}

void AuthCustom::initUi()
{
    qInfo() << Q_FUNC_INFO << "AuthCustom initUi";
    if (!m_module)
        return;

    if (!m_module->content() || m_module->content()->parent() == nullptr) {
        qInfo() << Q_FUNC_INFO << "m_module->init()";
        m_module->init();
        m_mainLayout->addWidget(m_module->content());
        setFocusProxy(m_module->content());
    }

    updateConfig();
}

void AuthCustom::reset()
{
    qInfo() << Q_FUNC_INFO << "Reset custom auth";
    m_currentAuthData = AuthCallbackData();
    if (m_module)
        m_module->init();
}


void AuthCustom::setAuthState(const int state, const QString &result)
{
    qInfo() << Q_FUNC_INFO << "AuthCustom::setAuthState:" << state << result;
    m_state = state;
    switch (state) {
    case AuthCommon::AS_Started:
        if (m_model->appType() == AuthCommon::AppType::Lock) {
            sendAuthToken();
        }
        break;
    case AuthCommon::AS_Success:
        emit authFinished(state);
        break;
    default:
        break;
    }
}

void AuthCustom::setAuthData(const AuthCallbackData &callbackData)
{
    m_currentAuthData = callbackData;
    if (AuthResult::Success != callbackData.result) {
        qWarning() << "Custom auth result is not success";
        return;
    }

    const QString &account = QString::fromStdString(callbackData.account);
    if (!account.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "Request check account: " << account;
        emit requestCheckAccount(account);
    } else {
        emit requestSendToken("");
    }
}

void AuthCustom::authCallBack(const AuthCallbackData *callbackData, void *app_data)
{
    qInfo() << Q_FUNC_INFO << "AuthCustom::authCallBack";
    AuthCustom *authCustom = static_cast<AuthCustom *>(app_data);
    if (callbackData && authCustom) {
        authCustom->setAuthData(*callbackData);
    }
}

std::string AuthCustom::messageCallback(const std::string &message, void *app_data)
{
    qDebug() << Q_FUNC_INFO << "Recieved message: " << QString::fromStdString(message);
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(QByteArray::fromStdString(message), &jsonParseError);

    QJsonObject retObj;
    QJsonObject dataObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        retObj["Code"] = -1;
        retObj["Message"] = "Failed to analysis message info";
        qWarning() << "Failed to analysis message info from plugin!: " << QString::fromStdString(message);
        return toJson(retObj).toStdString();
    }

    QJsonObject messageObj = messageDoc.object();
    AuthCustom *authCustom = static_cast<AuthCustom *>(app_data);
    if (!authCustom) {
        retObj["Code"] = -1;
        retObj["Message"] = "App data is nullptr!";
    }

    const SessionBaseModel *model = authCustom->getModel();
    if (!model) {
        retObj["Code"] = -1;
        retObj["Message"] = "Data model is nullptr!";
        return toJson(retObj).toStdString();
    }

    QString cmdType = messageObj.value("CmdType").toString();
    qInfo() << "Cmd type: " << cmdType;
    if (cmdType == "GetProperties") {
        QJsonArray properties;
        properties = messageObj.value("Data").toArray();

        if (properties.contains("AppType")) {
            dataObj["AppType"] = model->appType();
        }

        if (properties.contains("CurrentUser")) {
            if (model->currentUser()) {
                QJsonObject user;
                user["Name"] = model->currentUser()->name();
                dataObj["CurrentUser"] = user;
            } else {
                retObj["Code"] = -1;
                retObj["Message"] = "Current user is null!";
            }
        }
    }
    else if (cmdType == "setAuthTypeInfo")
    {
        QJsonObject dataObj = messageObj.value("Data").toObject();
        AuthCommon::AuthType type = (AuthCommon::AuthType)dataObj["AuthType"].toInt();
        AuthCustom *authCustom = static_cast<AuthCustom *>(app_data);
        if (authCustom) {
            authCustom->changeAuthType(type);
        }
    }

    retObj["Data"] = dataObj;
    return toJson(retObj).toStdString();
}

void AuthCustom::changeAuthType(AuthCommon::AuthType type)
{
    emit notifyAuthTypeChange(type);
}

dss::module::LoginModuleInterface* AuthCustom::getModule() const
{
    return m_module;
}

bool AuthCustom::event(QEvent *e)
{
    if(e->type() == QEvent::Resize || e->type() == QEvent::Show) {
        emit notifyResizeEvent();

        if (e->type() == QEvent::Show) {
            if (m_module->content()->parent() != this) {
                m_mainLayout->addWidget(m_module->content());
                setFocusProxy(m_module->content());
            }
        }
    }
    return AuthModule::event(e);
}

void AuthCustom::updateConfig()
{
    if (!m_module)
        return;

    QJsonObject message;
    message["CmdType"] = "GetConfigs";
    std::string result = m_module->onMessage(toJson(message).toStdString());
    qInfo() << "Plugin result: " << QString::fromStdString(result);

    QJsonParseError jsonParseError;
    const QJsonDocument resultDoc = QJsonDocument::fromJson(QByteArray::fromStdString(result), &jsonParseError);
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
    m_showAvatar = dataObj["ShowAvatar"].toBool(m_showAvatar);
    m_showUserName = dataObj["ShowUserName"].toBool(m_showUserName);
    m_showSwitchButton = dataObj["ShowSwitchButton"].toBool(m_showSwitchButton);
    m_showLockButton = dataObj["ShowLockButton"].toBool(m_showLockButton);
    m_defaultAuthLevel = (AuthCommon::DefaultAuthLevel)dataObj["DefaultAuthLevel"].toInt();
    m_authType = (AuthCommon::AuthType)dataObj["AuthType"].toInt();
}

QSize AuthCustom::contentSize() const
{
    if (!m_module || !m_module->content())
        return QSize();

    return m_module->content()->size();
}

void AuthCustom::sendAuthToken()
{
    if (m_currentAuthData.result == AuthResult::Success) {
        Q_EMIT requestSendToken(QString::fromStdString(m_currentAuthData.token));
    } else {
        qWarning() << "Current validation is not successfully";
    }
    reset();
}

void AuthCustom::lightdmAuthStarted()
{
    if (!m_module)
        return;

    QJsonObject message;
    message["CmdType"] = "StartAuth";
    message["AuthObjectType"] = AuthObjectType::LightDM;
    std::string result = m_module->onMessage(toJson(message).toStdString());
    qInfo() << "Plugin result: " << QString::fromStdString(result);
}
