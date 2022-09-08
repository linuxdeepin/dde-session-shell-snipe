// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_module.h"

#include <QBoxLayout>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDBusConnection>
#include <QDBusMessage>

namespace dss {
namespace module {

LoginModule::LoginModule(QObject *parent)
    : QObject(parent)
    , m_callbackFun(nullptr)
    , m_callbackData(new AuthCallbackData)
    , m_loginWidget(nullptr)
{
    setObjectName(QStringLiteral("LoginModule"));

    m_callbackData->account = "uos";
    m_callbackData->token = "1";
    m_callbackData->result = 0;
}

LoginModule::~LoginModule()
{
    if (m_loginWidget) {
        delete m_loginWidget;
    }
    if (m_callbackData) {
        delete m_callbackData;
    }
}

void LoginModule::init()
{
    initUI();
}

void LoginModule::reset()
{
    init();
}

void LoginModule::initUI()
{
    if (m_loginWidget) {
        return;
    }
    m_loginWidget = new QWidget;
    m_loginWidget->setAccessibleName(QStringLiteral("LoginWidget"));
    m_loginWidget->setMinimumSize(260, 100);

    m_loginWidget->setLayout(new QHBoxLayout);
    QPushButton *passButton = new QPushButton("Validation passed", m_loginWidget);
    passButton->setFixedSize(160, 40);
    m_loginWidget->layout()->addWidget(passButton);

    connect(passButton, &QPushButton::clicked, this, [this] {
        AuthCallbackData data;
        data.account = "uos";
        data.token = "1";
        data.result = AuthResult::Success;
        m_callbackFun(&data, m_callback->app_data);
    }, Qt::DirectConnection);

}

void LoginModule::setCallback(LoginCallBack *callback)
{
    m_callback = callback;
    m_callbackFun = callback->authCallbackFun;
}

} // namespace module
} // namespace dss
