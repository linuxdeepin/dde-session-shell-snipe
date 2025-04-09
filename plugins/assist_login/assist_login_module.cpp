// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assist_login_module.h"

namespace dss {
namespace module {

AssistLoginModule::AssistLoginModule(QObject *parent)
    : QObject(parent)
    , m_callbackFun(nullptr)
    , m_callbackData(new AuthCallbackData)
    , m_messageCallbackFunc(nullptr)
    , m_work(nullptr)
    , m_mainWidget(nullptr)
{
    setObjectName(QStringLiteral("AssistLoginModule"));
}

AssistLoginModule::~AssistLoginModule()
{
    if (m_mainWidget) {
        delete m_mainWidget;
    }
    if (m_callbackData) {
        delete m_callbackData;
    }
}

void AssistLoginModule::init()
{
    m_work = new AssistLoginWork(this);
    m_work->moveToThread(qApp->thread());
    initUI();
    initConnect();
}

void AssistLoginModule::reset()
{
    init();
}

void AssistLoginModule::initUI()
{
    if (m_mainWidget) {
        return;
    }

    m_mainWidget = new AssistLoginWidget;
}

void AssistLoginModule::setCallback(LoginCallBack *callback)
{
    m_callback = callback;
    m_callbackFun = callback->authCallbackFun;
}

void AssistLoginModule::initConnect()
{
    if (!m_mainWidget || !m_work) {
        return;
    }
    connect(m_work, &AssistLoginWork::sendAuthToken, this, &AssistLoginModule::sendAuthToken);
    connect(m_mainWidget, &AssistLoginWidget::sendAuth, this, [this](const QString & account, const QString & token) {
        if (m_callbackFun == nullptr) {
            return;
        }
        AuthCallbackData data;
        data.account = account.toStdString();
        data.token = token.toStdString();
        data.result = AuthResult::Success;

        m_callbackFun(&data, m_callback->app_data);
    }, Qt::DirectConnection);

    connect(m_mainWidget, &AssistLoginWidget::startService, m_work, &AssistLoginWork::onStartService);
    connect(m_mainWidget, &AssistLoginWidget::stopService, m_work, &AssistLoginWork::onStopService);
}

std::string AssistLoginModule::onMessage(const std::string &message)
{
    qInfo() << Q_FUNC_INFO;
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(QByteArray::fromStdString(message), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Failed to obtain message from shell!: " << QString::fromStdString(message);
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

    } else if (cmdType == "GetConfigs") {
        QJsonObject retDataObj;
        retDataObj["ShowAvatar"] = false;
        retDataObj["ShowUserName"] = false;
        retDataObj["ShowSwitchButton"] = false;
        retDataObj["ShowLockButton"] = false;
        retDataObj["DefaultAuthLevel"] = DefaultAuthLevel::StrongDefault;
        retDataObj["AuthType"] = AuthType::AT_Password;

        retObj["Data"] = retDataObj;
    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson().toStdString();
}

void AssistLoginModule::sendAuthToken(const QString &user, const QString &passwd)
{
    if (m_callbackFun == nullptr) {
        return;
    }
    AuthCallbackData data;
    data.account = user.toStdString();
    data.token = passwd.toStdString();
    data.result = AuthResult::Success;

    m_callbackFun(&data, m_callback->app_data);
}

}
}
