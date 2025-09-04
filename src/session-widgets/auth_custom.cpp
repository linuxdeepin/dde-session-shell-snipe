// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_custom.h"

#include "modules_loader.h"
#include "sessionbasemodel.h"

#include <QBoxLayout>

using namespace dss::module;

QList<AuthCustom*> AuthCustom::AuthCustomObjs = {};

AuthCustom::AuthCustom(QWidget *parent, AuthCommon::AuthType type)
    : AuthModule(type, parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_plugin(nullptr)
    , m_model(nullptr)
{
    setObjectName(QStringLiteral("AuthCustom"));
    setAccessibleName(QStringLiteral("AuthCustom"));

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    AuthCustomObjs.append(this);

    // 需要父类的定时解锁
    AuthModule::initConnections();
}

AuthCustom::~AuthCustom()
{
    detachPlugin();
    // FIXME 这种处理方式不通用，应该由插件请求登陆器把自己卸载掉
    AuthCustomObjs.removeAll(this);
}

void AuthCustom::detachPlugin()
{
    qCInfo(DDE_SHELL) << "Detach plugin";
    if (m_plugin && m_plugin->content() && m_plugin->content()->parent() == this) {
        m_plugin->content()->setParent(nullptr);
    }

    m_plugin = nullptr;
}

void AuthCustom::setModule(LoginPlugin *module)
{
    if (m_plugin) {
        return;
    }
    m_plugin = module;

    setCallback();
}

void AuthCustom::setModel(const SessionBaseModel *model)
{
    if (!model || m_model == model)
        return;

    m_model = model;
    // 通知插件当前用户发生变化
    connect(m_model, &SessionBaseModel::currentUserChanged, this, [this](const std::shared_ptr<User> currentUser) {
        if (!currentUser || !m_plugin)
            return;

        m_plugin->notifyCurrentUserChanged(currentUser->name(), currentUser->uid());
    });
}

void AuthCustom::setCallback()
{
    if (!m_plugin) {
        qCWarning(DDE_SHELL) << "Plugin module is null";
        return;
    }
    m_plugin->setMessageCallback(&AuthCustom::messageCallback);
    m_plugin->setAppData(this);
    m_plugin->setAuthCallback(&AuthCustom::authCallback);
}

void AuthCustom::initUi()
{
    if (!m_plugin)
        return;

    if (!m_plugin->content() || m_plugin->content()->parent() == nullptr) {
        qCInfo(DDE_SHELL) << "Init module";
        m_plugin->init();
        m_mainLayout->addWidget(m_plugin->content());
        setFocusProxy(m_plugin->content());
    }

    updateConfig();
}

void AuthCustom::resetAuth()
{
    m_currentAuthData = LoginPlugin::AuthCallbackData();
}

void AuthCustom::reset()
{
    if (m_plugin)
        m_plugin->reset();
}

void AuthCustom::setAuthState(const AuthCommon::AuthState state, const QString &result)
{
    m_state = state;
    switch (state) {
    case AuthCommon::AS_Started:
        // greeter需要等lightdm开启验证后再发送认证信息
        if (m_model->appType() == AuthCommon::AppType::Lock) {
            // FIXME: 并不是所有插件都会有此场景下发token的需求,因此注意清空authdata
            sendAuthToken();
        }
        break;
    case AuthCommon::AS_Success:
        emit authFinished(state);
        break;
    case AuthCommon::AS_Unlocked:
        // 切换用户时，会有解锁的动作，因此需要响应unlock
        emit activeAuth(m_type);
        break;
    default:
        break;
    }
}

void AuthCustom::setAuthData(const LoginPlugin::AuthCallbackData &callbackData)
{
    m_currentAuthData = callbackData;
    if (LoginPlugin::AuthResult::Success != callbackData.result) {
        qCWarning(DDE_SHELL) << "Custom auth result is not success";
        return;
    }

    const QString &account = m_currentAuthData.account;
    if (!account.isEmpty()) {
        qCInfo(DDE_SHELL) << "Request check account: " << account;
        emit requestCheckAccount(account, m_plugin->pluginConfig().switchUserWhenCheckAccount);
    } else {
        // 如果插件的token信息带上了用户名，则通过checkAccount信号通知外面去做用户切换或发送token
        // 隐式地要求插件仅在主动切换用户时需要填充token的account，注意需要在代码中增加信号响应，参见sfawidget
        Q_EMIT requestSendToken(m_currentAuthData.token);
        resetAuth();
    }
}

void AuthCustom::authCallback(const LoginPlugin::AuthCallbackData *callbackData, void *app_data)
{
    qCInfo(DDE_SHELL) << "Receive auth callback";
    AuthCustom *authCustom = getAuthCustomObj(app_data);
    if (callbackData && authCustom) {
        authCustom->setAuthData(*callbackData);
    }
}

QString AuthCustom::messageCallback(const QString &message, void *app_data)
{
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(message.toLatin1(), &jsonParseError);

    QJsonObject retObj;
    QJsonObject dataObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        retObj["Code"] = -1;
        retObj["Message"] = "Failed to analysis message info";
        qCWarning(DDE_SHELL) << "Failed to analysis message from plugin: " << message;
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
    qCInfo(DDE_SHELL) << "Cmd type: " << cmdType;
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
                user["Uid"] = static_cast<int>(model->currentUser()->uid());
                dataObj["CurrentUser"] = user;
            } else {
                retObj["Code"] = -1;
                retObj["Message"] = "Current user is null!";
            }
        }
    } else if (cmdType == "setAuthTypeInfo") {
        QJsonObject dataObj = messageObj.value("Data").toObject();
        authCustom->changeAuthType(AUTH_TYPE_CAST(dataObj["AuthType"].toInt()));
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
    // 多个自定义插件时需要匹配当前的类型
    auto tmpType = type;
    if (type == AuthCommon::AT_Custom)
        tmpType = static_cast<AuthCommon::AuthType>(customAuthType());

    emit notifyAuthTypeChange(tmpType);
}

LoginPlugin* AuthCustom::getLoginPlugin() const
{
    return m_plugin;
}

QString AuthCustom::loginPluginKey() const
{
    if (m_plugin)
        return m_plugin->key();

    return QStringLiteral();
}

bool AuthCustom::event(QEvent *e)
{
    if(e->type() == QEvent::Resize || e->type() == QEvent::Show) {
        emit notifyResizeEvent();

        if (e->type() == QEvent::Show) {
            if (m_plugin && m_plugin->content() && m_plugin->content()->parent() != this) {
                m_mainLayout->addWidget(m_plugin->content());
                setFocusProxy(m_plugin->content());
                // 重新设置传给插件的appData
                setCallback();
            }
        }
    }
    return AuthModule::event(e);
}

// 响应解锁
void AuthCustom::updateUnlockPrompt()
{
    // DA不会主动刷新limit信息，以当前解锁时间判断
    if (m_integerMinutes == 0) {
        QTimer::singleShot(1000, this, [this] {
            emit activeAuth(m_type);
        });
        qCInfo(DDE_SHELL) << "Waiting authentication service...";
    }
}

void AuthCustom::updateConfig()
{
    if (!m_plugin)
        return;

    m_plugin->updateConfig();
}

QSize AuthCustom::contentSize() const
{
    if (!m_plugin || !m_plugin->content())
        return QSize();

    return m_plugin->content()->size();
}

QSize AuthCustom::contentSizeHint() const
{
    if (!m_plugin || !m_plugin->content())
        return QSize();

    return m_plugin->content()->sizeHint();
}

LoginPlugin::PluginConfig AuthCustom::pluginConfig() const
{
    if (!m_plugin)
        return LoginPlugin::PluginConfig();

    return m_plugin->pluginConfig();
}

void AuthCustom::sendAuthToken()
{
    qCInfo(DDE_SHELL) << "Send auth token";
    if (m_currentAuthData.result == LoginPlugin::AuthResult::Success) {
        Q_EMIT requestSendToken(m_currentAuthData.token);
    } else {
        qCWarning(DDE_SHELL) << "Current validation is not successfully";
    }
    resetAuth();
}

void AuthCustom::lightdmAuthStarted()
{
    if (!m_plugin)
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
    QString result = m_plugin->message(toJson(message));
    qCInfo(DDE_SHELL) << "Plugin message: " << result;
}

void AuthCustom::notifyAuthState(AuthCommon::AuthFlags authType, AuthCommon::AuthState state)
{
    qCInfo(DDE_SHELL) << "Notify auth state, auth type: " << authType << ", auth state: " << state;
    if (!m_plugin)
        return;

    QJsonObject message;
    message["CmdType"] = "AuthState";
    QJsonObject retDataObj;
    retDataObj["AuthType"] = static_cast<int>(authType);
    retDataObj["AuthState"] = state;
    message["Data"] = retDataObj;
    m_plugin->message(toJson(message));
}

/**
 * @brief 将限制信息发给插件
 *
 * @param limitsInfoStr 插件信息的json数据
 */
void AuthCustom::setLimitsInfo(const QMap<int, User::LimitsInfo> &limitsInfo)
{
    // 把输密码错误五次后，是否锁定的信息传给插件
    if (!m_plugin)
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
    m_plugin->message(toJson(message));
}

void AuthCustom::setLimitsInfo(const LimitsInfo &limitsInfo)
{
    if (!m_plugin)
        return;

    QJsonObject message;
    message["CmdType"] = "LimitsInfo";

    // FIXME: 注意limitsinfo这个结构体没有必要存在两个定义
    QJsonObject obj;
    obj["Locked"] = limitsInfo.locked;
    obj["MaxTries"] = static_cast<int>(limitsInfo.maxTries);
    obj["NumFailures"] = static_cast<int>(limitsInfo.numFailures);
    obj["UnlockSecs"] = static_cast<int>(limitsInfo.unlockSecs);;
    obj["UnlockTime"] = limitsInfo.unlockTime;
    message["Data"] = obj;
    m_plugin->message(toJson(message));

    AuthModule::setLimitsInfo(limitsInfo);
}

QJsonObject AuthCustom::getRootObj(const QString &jsonStr)
{
    QJsonParseError jsonParseError;
    const QJsonDocument &resultDoc = QJsonDocument::fromJson(jsonStr.toLocal8Bit(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || resultDoc.isEmpty()) {
        qCWarning(DDE_SHELL) << "Result json parse error: " << jsonParseError.error;
        return QJsonObject();
    }

    return resultDoc.object();
}

QJsonObject AuthCustom::getDataObj(const QString &jsonStr)
{
    const QJsonObject &rootObj = getRootObj(jsonStr);
    if (!rootObj.contains("Data")) {
        qCWarning(DDE_SHELL) << "Result doesn't contains the 'data' field";
        return QJsonObject();
    }

    return rootObj["Data"].toObject();
}
