// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_managed_login_module.h"

#include <QBoxLayout>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QApplication>

namespace dss {
namespace module_v2 {

FullManagedLoginModule::FullManagedLoginModule(QObject *parent)
    : QObject(parent)
    , m_appData(nullptr)
    , m_authCallback(nullptr)
    , m_messageCallback(nullptr)
    , m_loginWidget(nullptr)
    , m_appType(AppType::Lock)
    , m_isActive(true)
{
    setObjectName(QStringLiteral("FullManagedLoginModule"));
}

FullManagedLoginModule::~FullManagedLoginModule()
{
    if (m_loginWidget) {
        delete m_loginWidget;
    }
}

void FullManagedLoginModule::init()
{
    initUI();
    updateInfo();
}

void FullManagedLoginModule::reset()
{
    init();
}

void FullManagedLoginModule::initUI()
{
    if (m_loginWidget) {
        emit m_loginWidget->reset();
        return;
    }

    m_loginWidget = new FullManagedLoginWidget();
    m_loginWidget->setFixedSize(362, 420);
    QObject::connect(m_loginWidget, &FullManagedLoginWidget::sendAuthToken, this, [this](const QString &account, const QString &token) {
        if (QApplication::applicationName().contains("lock")) {
            if (m_userName != account && m_userName != "...") {
                qDebug() << "User name mismatch, user name: " << m_userName;
                m_loginWidget->showMessage("The user name does not match the current user, please check and re verify");
                return;
            }
        }

        qDebug() << "send Auth Token: " << account;
        AuthCallbackData data;
        data.account = account;
        data.token = token;
        data.result = 1;
        m_authCallback(&data, m_appData);
    },
                     Qt::DirectConnection);

    connect(m_loginWidget, &FullManagedLoginWidget::deactived, this, [this] {
        m_isActive = false;
        QTimer::singleShot(DEACTIVE_TIME_INTERVAL, this, [this] {
            m_isActive = true;
        });
    });
}

void FullManagedLoginModule::setAppData(AppDataPtr appData)
{
    m_appData = appData;
}

void FullManagedLoginModule::setAuthCallback(AuthCallbackFun authCallback)
{
    m_authCallback = authCallback;
}

void FullManagedLoginModule::setMessageCallback(MessageCallbackFunc messageCallback)
{
    m_messageCallback = messageCallback;
}

void FullManagedLoginModule::updateInfo()
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
        m_userName = user.value("Name").toString();
        qInfo() << "Current user: " << user;
    }
}

QString FullManagedLoginModule::message(const QString &message)
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
        retDataObj["ShowAvatar"] = false;
        retDataObj["ShowUserName"] = false;
        retDataObj["ShowSwitchButton"] = false;
        retDataObj["ShowLockButton"] = false;
        retDataObj["DefaultAuthLevel"] = DefaultAuthLevel::Default;
        retDataObj["AuthType"] = AuthType::AT_Custom;
        retObj["Data"] = retDataObj;
    } else if (cmdType == "AuthState") {
        int authType = data.value("AuthType").toInt();
        int authState = data.value("AuthState").toInt();
        // 所有类型验证成功，重置插件状态
        if (authType == AuthType::AT_All && authState == AuthState::AS_Success) {
            init();
        }
    } else if (cmdType == "StartAuth") {
        Q_EMIT m_loginWidget->greeterAuthStarted();
    } else if (cmdType == "IsPluginEnabled") {
        QJsonObject retDataObj;
        retDataObj["IsPluginEnabled"] = m_isActive;
        retObj["Data"] = retDataObj;
    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson();
}

} // namespace module_v2
} // namespace dss
