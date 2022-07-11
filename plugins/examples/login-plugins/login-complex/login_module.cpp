/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     YinJie <yinjie@uniontech.com>
*
* Maintainer: YinJie <yinjie@uniontech.com>
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
    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson().toStdString();
}

} // namespace module
} // namespace dss
