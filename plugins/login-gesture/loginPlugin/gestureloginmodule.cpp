// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gestureloginmodule.h"
#include "waypointmodel.h"
#include "modulewidget.h"
#include "userservice.h"
#include "gestureauthworker.h"
#include "gesturemodifyworker.h"

using namespace gestureLogin;
using namespace gestureSetting;

namespace dss {
namespace module_v2 {

GestureLoginModule::GestureLoginModule(QObject *parent)
    : QObject(parent)
    , m_appData(nullptr)
    , m_authCallback(nullptr)
    , m_messageCallback(nullptr)
    , m_loginWidget(nullptr)
    , m_appType(AppType::Lock)
{
    setObjectName(QStringLiteral("gesture-login-plugin"));
    // CAUTION: do not call initui here
    // when object created, it's not in GUI thread
}

GestureLoginModule::~GestureLoginModule()
{
    if (m_loginWidget) {
        delete m_loginWidget.data();
    }
}

void GestureLoginModule::init()
{
    updateInfo();
    initWorker();
}

QWidget *GestureLoginModule::content()
{
    return m_loginWidget;
}

void GestureLoginModule::reset()
{
    initUI();
    init();
}

void GestureLoginModule::sendToken(const QString &token)
{
    AuthCallbackData data;
    data.account = "";
    data.token = token;
    data.result = 1; // 固定值，代表本地验证通过,通知dss去校验密码
    m_authCallback(&data, m_appData);
}

void GestureLoginModule::enroll(const QString &token)
{
    UserService::instance()->enroll(token);
}

void GestureLoginModule::initUI()
{
    if (m_loginWidget) {
        qobject_cast<ModuleWidget *>(m_loginWidget)->reset();
        return;
    }

    m_loginWidget = new ModuleWidget();
}

void GestureLoginModule::setAppData(AppDataPtr appData)
{
    m_appData = appData;
}

void GestureLoginModule::setAuthCallback(AuthCallbackFun authCallback)
{
    m_authCallback = authCallback;
}

void GestureLoginModule::setMessageCallback(MessageCallbackFunc messageCallback)
{
    m_messageCallback = messageCallback;
}

void GestureLoginModule::updateInfo()
{
    if (!m_messageCallback) {
        qWarning() << "message callback func is nullptr";
        return;
    }

    // 发送获取属性的请求
    QJsonObject message;
    message.insert("CmdType", "GetProperties");
    QJsonArray array;
    array.append("AppType");
    array.append("CurrentUser");
    message["Data"] = array;
    QJsonDocument doc;
    doc.setObject(message);
    QString ret = m_messageCallback(doc.toJson(), m_appData);

    // 解析返回值
    QJsonParseError jsonParseError;
    const QJsonDocument retDoc = QJsonDocument::fromJson(ret.toLatin1(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || retDoc.isEmpty()) {
        qWarning() << "Failed to analysis AppType info from shell!: " << ret;
        return;
    }

    QJsonObject obj = retDoc.object();
    if (obj.value("Code").toInt() != 0) {
        qWarning() << "Get properties failed, message: " << obj.value("Message").toString();
        return;
    }

    QJsonObject data = obj.value("Data").toObject();
    if (data.contains("AppType")) {
        m_appType = data.value("AppType").toInt();
        qInfo() << "App type: " << m_appType;
    }

    if (data.contains("CurrentUser")) {
        QJsonObject user = data.value("CurrentUser").toObject();
        updateUser(user.value("Name").toString());
        qInfo() << "Current user: " << user;
    }
}

void GestureLoginModule::initWorker()
{
    UserService::instance()->setUserName(m_userName);

    auto model = WayPointModel::instance();
    auto gestureState = model->getGestureState();

    // 无手势，直接开始录入
    if (gestureState == GestureState::NotSet || gestureState == GestureState::SetAndRemoved) {
        model->setCurrentMode(Mode::Enroll);
    } else {
        model->setCurrentMode(Mode::Auth);
    }

    // 创建认证流程控制
    auto connectType = Qt::AutoConnection | Qt::UniqueConnection;
    // 注意只创建一个对象
    static GestureAuthWorker *authWorker = new GestureAuthWorker(this);
    authWorker->setActive(model->currentMode() == Mode::Auth);
    connect(this, &GestureLoginModule::onAuthStateChanged, authWorker, &GestureAuthWorker::onAuthStateChanged, Qt::ConnectionType(connectType));
    connect(this, &GestureLoginModule::onLimitsInfoChanged, authWorker, &GestureAuthWorker::onLimitsInfoChanged, Qt::ConnectionType(connectType));
    connect(authWorker, &GestureAuthWorker::requestSendToken, this, &GestureLoginModule::sendToken, Qt::ConnectionType(connectType));

    // 创建录入流程控制
    static GestureModifyWorker *enrollWorker = new GestureModifyWorker(this);
    enrollWorker->setActive(model->currentMode() == Mode::Enroll);
    connect(enrollWorker, &GestureModifyWorker::requestSaveToken, this, &GestureLoginModule::enroll, Qt::ConnectionType(connectType));
}

// 是否可以由dss去控制这个配置变化
void GestureLoginModule::updateUser(const QString &userName)
{
    if (m_userName == userName) {
        return;
    }

    m_userName = userName;
    initWorker();
}

QString GestureLoginModule::message(const QString &message)
{
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(message.toLatin1(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        qWarning() << "Failed to obtain message from shell!: " << message;
        return "";
    }

    QJsonObject retObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";

    QJsonObject msgObj = messageDoc.object();
    QString cmdType = msgObj.value("CmdType").toString();
    QJsonObject data = msgObj.value("Data").toObject();
    qInfo() << "Cmd type: " << cmdType;
    if (cmdType == "CurrentUserChanged") {
        updateUser(data.value("Name").toString());
        qDebug() << "Current user changed, user name: " << m_userName;
    } else if (cmdType == "GetConfigs") {
        QJsonObject retDataObj;
        retDataObj["ShowAvatar"] = false;
        retDataObj["ShowUserName"] = false;
        retDataObj["ShowSwitchButton"] = false;
        retDataObj["ShowLockButton"] = false;
        retDataObj["DefaultAuthLevel"] = DefaultAuthLevel::Default;
        retDataObj["DisableOtherAuthentications"] = false;
        retDataObj["AuthType"] = AuthType::AT_Pattern;
        retObj["Data"] = retDataObj;
    } else if (cmdType == "AuthState") {
        int authType = data.value("AuthType").toInt();
        int authState = data.value("AuthState").toInt();
        // 处理认证结果
        // 注意录入相关不在此处理
        Q_EMIT onAuthStateChanged(authType, authState);
    } else if (cmdType == "LimitsInfo") {
        bool locked = data.value("Locked").toBool();
        int maxTries = data.value("MaxTries").toInt();
        int numFailures = data.value("NumFailures").toInt();
        int unlockSecs = data.value("UnlockSecs").toInt();
        QString unlockTime = data.value("UnlockTime").toString();
        Q_EMIT onLimitsInfoChanged(locked, maxTries, numFailures, unlockSecs, unlockTime);
    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson();
}

} // namespace module_v2
} // namespace dss
