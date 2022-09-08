// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_module.h"

#include <QBoxLayout>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>

namespace dss {
namespace module {

LoginModule::LoginModule(QObject *parent)
    : QObject(parent)
    , m_callback(nullptr)
    , m_authCallbackFun(nullptr)
    , m_messageCallbackFunc(nullptr)
    , m_loginWidget(nullptr)
    , m_appType(AppType::Lock)
{
    setObjectName(QStringLiteral("LoginModule"));
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
    QObject::connect(m_loginWidget, &LoginWidget::sendAuthToken, this, [this] (const QString &account, const QString &token) {
        if (m_userName != account && m_userName != "...") {
            qDebug() << "User name mismatch, user name: " << m_userName;
            m_loginWidget->showMessage("The user name does not match the current user, please check and re verify");
            return;
        }
        qDebug() << "send Auth Token: " << account;
        AuthCallbackData data;
        data.account = account.toStdString();
        data.token = token.toStdString();
        data.result = 1;
        m_authCallbackFun(&data, m_callback->app_data);
    }, Qt::DirectConnection);
}

void LoginModule::updateInfo()
{
    if (!m_messageCallbackFunc) {
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
    std::string ret = m_messageCallbackFunc(doc.toJson().toStdString(), m_callback->app_data);

    // 解析返回值
    QJsonParseError jsonParseError;
    const QJsonDocument retDoc = QJsonDocument::fromJson(QByteArray::fromStdString(ret), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || retDoc.isEmpty()) {
        qWarning() << "Failed to analysis AppType info from shell!: " << QString::fromStdString(ret);
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

void LoginModule::setCallback(LoginCallBack *callback)
{
    m_callback = callback;
    m_authCallbackFun = callback->authCallbackFun;
    m_messageCallbackFunc = callback->messageCallbackFunc;
}

std::string LoginModule::onMessage(const std::string &message)
{
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(QByteArray::fromStdString(message), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        qWarning() << "Failed to obtain message from shell!: " << QString::fromStdString(message);
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
    return doc.toJson().toStdString();
}

} // namespace module
} // namespace dss
