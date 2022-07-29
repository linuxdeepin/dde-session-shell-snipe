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

        m_module->message(toJson(message));
    });
}

void AuthCustom::setCallback()
{
    if (!m_module) {
        qWarning() << "Plugin module is null";
        return;
    }
    m_module->setAuthCallback(&AuthCustom::authCallback);
    m_module->setMessageCallback(&AuthCustom::messageCallback);
    m_module->setAppData(this);
}

void AuthCustom::initUi()
{
    qInfo() << Q_FUNC_INFO;
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

void AuthCustom::resetAuth()
{
    qInfo() << Q_FUNC_INFO;
    m_currentAuthData = AuthCallbackData();
}

void AuthCustom::reset()
{
    qInfo() << Q_FUNC_INFO;
    if (m_module)
        m_module->reset();
}

void AuthCustom::setAuthState(const int state, const QString &result)
{
    qInfo() << Q_FUNC_INFO << "AuthCustom::setAuthState:" << state << result;
    m_state = state;
    switch (state) {
    case AuthCommon::AS_Started:
        // greeter需要等lightdm开启验证后再发送认证信息
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

    const QString &account = callbackData.account;
    if (!account.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "Request check account: " << account;
        emit requestCheckAccount(account);
    } else {
        emit requestSendToken("");
    }
}

void AuthCustom::authCallback(const AuthCallbackData *callbackData, void *app_data)
{
    qInfo() << Q_FUNC_INFO << "AuthCustom::authCallback";
    AuthCustom *authCustom = getAuthCustomObj(app_data);
    if (callbackData && authCustom) {
        authCustom->setAuthData(*callbackData);
    }
}

QString AuthCustom::messageCallback(const QString &message, void *app_data)
{
    qDebug() << Q_FUNC_INFO << "Received message: " << message;
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(message.toLatin1(), &jsonParseError);

    QJsonObject retObj;
    QJsonObject dataObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        retObj["Code"] = -1;
        retObj["Message"] = "Failed to analysis message info";
        qWarning() << "Failed to analysis message info from plugin!: " << message;
        return toJson(retObj);
    }

    QJsonObject messageObj = messageDoc.object();
    AuthCustom *authCustom = getAuthCustomObj(app_data);
    if (!authCustom) {
        retObj["Code"] = -1;
        retObj["Message"] = "App data is nullptr!";
        return toJson(retObj);
    }

    const SessionBaseModel *model = authCustom->getModel();
    if (!model) {
        retObj["Code"] = -1;
        retObj["Message"] = "Data model is nullptr!";
        return toJson(retObj);
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
    return toJson(retObj);
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
    QString result = m_module->message(toJson(message));
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
    m_pluginConfig.defaultAuthLevel = (AuthCommon::DefaultAuthLevel)dataObj["DefaultAuthLevel"].toInt();
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
    qInfo() << "Send auth token";
    if (m_currentAuthData.result == AuthResult::Success) {
        Q_EMIT requestSendToken(m_currentAuthData.token);
    } else {
        qWarning() << "Current validation is not successfully";
    }
    resetAuth();
}

void AuthCustom::lightdmAuthStarted()
{
    qInfo() << Q_FUNC_INFO;
    if (!m_module)
        return;

    // 在用户A的界面登陆用户B，将结果返回后，首先会进行一次用户切换
    // 切换完成后把之前插件发送的验证结果进行一次验证
    sendAuthToken();

    // Tell plugin that lightdm authentication started
    QJsonObject message;
    message["CmdType"] = "StartAuth";
    QJsonObject retDataObj;
    retDataObj["AuthObjectType"] = AuthObjectType::LightDM;
    message["Data"] = retDataObj;
    QString result = m_module->message(toJson(message));
    qInfo() << "Plugin result: " << result;
}

void AuthCustom::notifyAuthState(AuthCommon::AuthType authType, AuthCommon::AuthState state)
{
    qInfo() << Q_FUNC_INFO << authType << ", auth state: " << state;
    if (!m_module)
        return;

    QJsonObject message;
    message["CmdType"] = "AuthState";
    QJsonObject retDataObj;
    retDataObj["AuthType"] = authType;
    retDataObj["AuthState"] = state;
    message["Data"] = retDataObj;
    m_module->message(toJson(message));
}

/**
 * @brief 将限制信息发给插件
 *
 * @param limitsInfoStr 插件信息的json数据
 */
void AuthCustom::setLimitsInfo(const QMap<int, User::LimitsInfo> &limitsInfo)
{
    // 把输密码错误五次后，是否锁定的信息传给插件
    if (!m_module)
        return;

    QJsonArray array;
    auto it = limitsInfo.constBegin();
    while (it != limitsInfo.end()) {
        array.append(it.value().toJson());
        ++it;
    }
    QJsonObject message;
    message["CmdType"] = "LimitsInfo";
    message["Data"] = array;
    m_module->message(toJson(message));
}

QJsonObject AuthCustom::getRootObj(const QString &jsonStr)
{
    QJsonParseError jsonParseError;
    const QJsonDocument &resultDoc = QJsonDocument::fromJson(jsonStr.toLocal8Bit(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || resultDoc.isEmpty()) {
        qWarning() << "Result json parse error";
        return QJsonObject();
    }

    return resultDoc.object();
}

QJsonObject AuthCustom::getDataObj(const QString &jsonStr)
{
    const QJsonObject &rootObj = getRootObj(jsonStr);
    if (!rootObj.contains("Data")) {
        qWarning() << "Result doesn't contains the 'data' field";
        return QJsonObject();
    }

    return rootObj["Data"].toObject();
}

bool AuthCustom::supportDefaultUser(dss::module::LoginModuleInterface *module)
{
    if (!module)
        return false;

    qInfo() << Q_FUNC_INFO;
    QJsonObject message;
    message["CmdType"] = "GetConfigs";
    const QString &result = module->message(toJson(message));
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty())
        return true;

    return dataObj["SupportDefaultUser"].toBool(true);
}

bool AuthCustom::isPluginEnabled(dss::module::LoginModuleInterface *module)
{
    if (!module)
        return false;

    qDebug() << Q_FUNC_INFO;
    QJsonObject message;
    message["CmdType"] = "IsPluginEnabled";
    const QString &result = module->message(toJson(message));
    qDebug() << "Result: " << result;
    const QJsonObject &dataObj = getDataObj(result);
    if (dataObj.isEmpty() || !dataObj.contains("IsPluginEnabled"))
        return true;

    return dataObj["IsPluginEnabled"].toBool(true);
}
