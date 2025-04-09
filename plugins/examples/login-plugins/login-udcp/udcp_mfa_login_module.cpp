// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "udcp_mfa_login_module.h"

#include <QDebug>

static QString success_reply(const QJsonValue &data)
{
    QJsonObject reply {
        {"Code", 0},
        {"Message", "Success"},
        {"Data", data}
    };
    return QJsonDocument(reply).toJson(QJsonDocument::Compact);
}

namespace dss {
namespace module_v2 {

UdcpMFALoginModule::UdcpMFALoginModule(QObject *parent)
    : QObject(parent)
{
    setObjectName("LoginModule");
}

UdcpMFALoginModule::~UdcpMFALoginModule() noexcept
{
    if (m_mainWindow) {
        delete m_mainWindow;
        m_mainWindow = nullptr;
    }
}

QString UdcpMFALoginModule::message(const QString &msg)
{
    qDebug() << Q_FUNC_INFO << "received a message:" << msg;

    auto doc = QJsonDocument::fromJson(msg.toUtf8());

    if (!doc.isObject()) {
        qWarning() << "given message is not an json object";
        return {};
    }

    auto obj = doc.object();
    auto cmd = obj.value("CmdType").toString();
    auto data = obj.value("Data").toObject();

    if (cmd == "CurrentUserChanged") {
        return success_reply({});

    } else if (cmd == "GetConfigs") {
        QJsonObject payload {
            {"ShowAvatar", true},
            {"ShowUserName", true},
            {"ShowSwitchButton", false},
            {"ShowLockButton", false},
            {"DefaultAuthLevel", DefaultAuthLevel::StrongDefault},
            {"DisableOtherAuthentications", true}};
        return success_reply(payload);

    } else if (cmd == "AuthState") {
        return success_reply({});

    } else if (cmd == "GetLevel") {
        QJsonObject payload {
            {"Level", 2}};
        return success_reply(payload);

    } else if (cmd == "StartAuth") {
        m_mainWindow->setFocus();
        return success_reply({});

    } else if (cmd == "GetLoginType") {
        QJsonObject payload {
            {"LoginType", 1}
        };
        return success_reply(payload);

    } else if (cmd == "GetSessionTimeout") {
        QJsonObject payload {
            {"SessionTimeout", 30 * 1000}
        };
        return success_reply(payload);

    } else if (cmd == "UpdateLoginType") {
        QJsonObject payload {
            {"LoginType", 0}
        };
        return success_reply(payload);


    } else if (cmd == "HasSecondLevel") {
        QJsonObject payload {
            {"HasSecondLevel", true}
        };
        return success_reply(payload);
    }

    return success_reply({});
}

void UdcpMFALoginModule::reset()
{
    qDebug() << Q_FUNC_INFO << "login module reset";
    init();
}

void UdcpMFALoginModule::init()
{
    initData();
    if (m_mainWindow) {
        m_mainWindow->reset();
    } else {
        initUI();
        initConnections();
    }
}

void UdcpMFALoginModule::initUI()
{
    m_mainWindow = new UdcpMFALoginWidget(m_user);
}

void UdcpMFALoginModule::initConnections()
{
    connect(m_mainWindow, &UdcpMFALoginWidget::finished, [this] {
        AuthCallbackData data;
        data.result = AuthResult::Success;
        data.account = m_user;
        data.token = "1";
        m_authCallback(&data, m_appData);
    });
}

void UdcpMFALoginModule::initData()
{
    if (!m_messageCallback) {
        qWarning() << "message callback func is null";
        return;
    }

    // 发送获取属性的请求
    QJsonArray dataArr {"AppType", "CurrentUser"};
    QJsonObject obj {
        {"CmdType", "GetProperties"},
        {"Data", dataArr}
    };

    auto result = m_messageCallback(QJsonDocument(obj).toJson(QJsonDocument::Compact), m_appData);
    qDebug() << "received property result:" << result;

    auto doc = QJsonDocument::fromJson(result.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    auto reply = doc.object();
    auto data = reply["Data"].toObject();
    auto user = data["CurrentUser"].toObject();
    m_user = user["Name"].toString();
}

}
}

