/*
 * Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
 *
 * Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
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
