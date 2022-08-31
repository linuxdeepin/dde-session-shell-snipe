// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_custom.h"

#include "modules_loader.h"
#include "sessionbasemodel.h"

#include <QBoxLayout>

using namespace dss::module;

QList<AuthCustom*> AuthCustom::AuthCustomObjs = {};

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

    AuthCustomObjs.append(this);
}

AuthCustom::~AuthCustom()
{
    if (m_module && m_module->content() && m_module->content()->parent() == this) {
        m_module->content()->setParent(nullptr);
    }

    // FIXME 这种处理方式不通用，应该由插件请求登陆器把自己卸载掉
    AuthCustomObjs.removeAll(this);
    if (AuthCustomObjs.isEmpty()) {
        if (m_module) {
            ModulesLoader::instance().removeModule(m_module->key());
        }
    }
}

void AuthCustom::setModule(dss::module::LoginModuleInterface *module)
{
    if (m_module) {
        return;
    }
    m_module = module;

    setCallback();
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

void AuthCustom::setCallback()
{
    dss::module::LoginCallBack callback;
    callback.app_data = this;
    callback.authCallbackFun = AuthCustom::authCallBack;
    callback.messageCallbackFunc = AuthCustom::messageCallback;
    if (m_module) {
        m_module->setCallback(&callback);
    }
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
    AuthCustom *authCustom = getAuthCustomObj(app_data);
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
    AuthCustom *authCustom = getAuthCustomObj(app_data);
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
    } else if (cmdType == "setAuthTypeInfo") {
        QJsonObject dataObj = messageObj.value("Data").toObject();
        AuthCommon::AuthType type = (AuthCommon::AuthType)dataObj["AuthType"].toInt();
        authCustom->changeAuthType(type);
    }

    retObj["Data"] = dataObj;
    return toJson(retObj).toStdString();
}

/**
 * @brief 将插件回传的void对象指针转换为AuthCustom指针
 *
 * 这样做的原因在于，如果存在屏幕插拔的情况，那么AuthCustom *会被释放掉，但是插件是不知道的（也不能依赖插件去做这个处理）
 * 后面插件把这个app_data回传到登录器，需要与AuthCustomObjs中保存的指针对比，判断app_data指针是否可用。
 *
 * @param app_data 从插件回传的AuthCustom对象指针
 * @return AuthCustom* AuthCustom对象指针
 */
// TODO 考虑多屏使用一个不会释放的对象管理插件
AuthCustom *AuthCustom::getAuthCustomObj(void *app_data)
{
    AuthCustom *authCustom = static_cast<AuthCustom *>(app_data);
    if (AuthCustomObjs.contains(authCustom)) {
        return authCustom;
    }

    if (AuthCustomObjs.size() > 0) {
        return AuthCustomObjs.first();
    }

    return nullptr;
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
            if (m_module && m_module->content() && m_module->content()->parent() != this) {
                m_mainLayout->addWidget(m_module->content());
                setFocusProxy(m_module->content());
                // 重新设置传给插件的appData
                setCallback();
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
