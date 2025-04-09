// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_module.h"

#include <QBoxLayout>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QJsonArray>

namespace dss {
namespace module_v2 {

LoginModule::LoginModule(QObject *parent)
    : QObject(parent)
    , m_appData(nullptr)
    , m_authCallback(nullptr)
    , m_messageCallback(nullptr)
    , m_loginWidget(nullptr)
    , m_appType(AppType::Lock)
{
    setObjectName(QStringLiteral("complex-login-plugin"));
}

LoginModule::~LoginModule()
{
    if (m_loginWidget) {
        delete m_loginWidget;
    }
}

void LoginModule::init()
{
    initUI();
    updateInfo();
}

void LoginModule::reset()
{
    init();
}

void LoginModule::initUI()
{
    if (m_loginWidget) {
        emit m_loginWidget->reset();
        return;
    }

    m_loginWidget = new LoginWidget();
    m_loginWidget->setFixedSize(362, 420);
    QObject::connect(m_loginWidget, &LoginWidget::sendAuthToken, this, [this](const QString &account, const QString &token) {
#if 0
        // 通常需要判断一下登录器当前的用户和插件正在验证的用户是否相同，具体根据需求而定。
        if (m_userName != account && m_userName != "...") {
            qDebug() << "User name mismatch, user name: " << m_userName;
            m_loginWidget->showMessage("The user name does not match the current user, please check and re verify");
            return;
        }
#endif
        qDebug() << "Send auth token: " << account;
        AuthCallbackData data;
        data.account = account;
        data.token = token;
        data.result = 1;
        m_authCallback(&data, m_appData);
    }, Qt::DirectConnection);
}

void LoginModule::setAppData(AppDataPtr appData)
{
    m_appData = appData;
}

void LoginModule::setAuthCallback(AuthCallbackFun authCallback)
{
    m_authCallback = authCallback;
}

void LoginModule::setMessageCallback(MessageCallbackFunc messageCallback)
{
    m_messageCallback = messageCallback;
}

void LoginModule::updateInfo()
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
        return ;
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
        m_userName = user.value("Name").toString();
        qInfo() << "Current user: " << user;
    }
}

QString LoginModule::message(const QString &message)
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
        m_userName = data.value("Name").toString();
        qDebug() << "Current user changed, user name: " << m_userName;
    } else if (cmdType == "GetConfigs") {
        QJsonObject retDataObj;
        retDataObj["ShowAvatar"] = true;
        retDataObj["ShowUserName"] = true;
        retDataObj["ShowSwitchButton"] = true;
        retDataObj["ShowLockButton"] = false;
        retDataObj["DefaultAuthLevel"] = DefaultAuthLevel::Default;
        retDataObj["DisableOtherAuthentications"] = true;
        retObj["Data"] = retDataObj;
    } else if (cmdType == "AuthState") {
        int authType = data.value("AuthType").toInt();
        int authState = data.value("AuthState").toInt();
        // 所有类型验证成功，重置插件状态
        if (authType == AuthType::AT_All && authState == AuthState::AS_Success) {
            init();
        }
    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson();
}

} // namespace module
} // namespace dss
