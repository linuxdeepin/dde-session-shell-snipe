// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login-widget.h"

#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent)
    , m_messageLabel(nullptr)
{
    setObjectName(QStringLiteral("LoginWidget"));
    setAccessibleName(QStringLiteral("LoginWidget"));

    init();
}

void LoginWidget::init()
{
    // init ui
    QLabel *userNameLabel = new QLabel("User Name: ", this);
    userNameLabel->setFixedHeight(35);
    QLineEdit *userNameEdit = new QLineEdit(this);
    userNameEdit->setFixedHeight(35);
    QHBoxLayout *userNameLayout = new QHBoxLayout();
    userNameLayout->addWidget(userNameLabel, 3, Qt::AlignRight);
    userNameLayout->addSpacing(15);
    userNameLayout->addWidget(userNameEdit, 7);
    setFocusProxy(userNameEdit);

    QLabel *tokenLabel = new QLabel("Token: ", this);
    tokenLabel->setFixedHeight(35);
    QLineEdit *tokenEdit = new QLineEdit(this);
    tokenEdit->setFixedHeight(35);
    QHBoxLayout *tokenLayout = new QHBoxLayout();
    tokenLayout->addWidget(tokenLabel, 3, Qt::AlignRight);
    tokenLayout->addSpacing(15);
    tokenLayout->addWidget(tokenEdit, 7);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setMinimumHeight(80);
    m_messageLabel->setWordWrap(true);
    QFont font = m_messageLabel->font();
    font.setPixelSize(30);
    m_messageLabel->setFont(font);
    m_messageLabel->setAlignment(Qt::AlignCenter);

    QPushButton *sendButton = new QPushButton("Send Token", this);
    sendButton->setFixedSize(120, 35);
    sendButton->setEnabled(false);
    sendButton->setDefault(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->setMargin(15);
    layout->addLayout(userNameLayout);
    layout->addSpacing(30);
    layout->addLayout(tokenLayout);
    layout->addSpacing(50);
    layout->addWidget(m_messageLabel);
    layout->addStretch(1);
    layout->addWidget(sendButton, 0, Qt::AlignCenter);
    layout->addSpacing(35);

    // init connections
    auto setButtonEnabledFunc = [sendButton, userNameEdit, tokenEdit] {
        const QString &userName = userNameEdit->text().trimmed();
        const QString &token = tokenEdit->text().trimmed();
        sendButton->setEnabled(!userName.isEmpty() && !token.isEmpty());
    };

    auto sendToken = [this, userNameEdit, tokenEdit] {
        const QString &userName = userNameEdit->text().trimmed();
        const QString &token = tokenEdit->text().trimmed();

        Q_EMIT sendAuthToken(userName, token);
    };

    connect(userNameEdit, &QLineEdit::textChanged, setButtonEnabledFunc);
    connect(tokenEdit, &QLineEdit::textChanged, setButtonEnabledFunc);

    connect(sendButton, &QPushButton::clicked, this, sendToken);
    connect(tokenEdit, &QLineEdit::returnPressed, this,  sendToken);
    connect(userNameEdit, &QLineEdit::returnPressed, this, [this, userNameEdit, tokenEdit, sendToken] {
        if (!userNameEdit->text().trimmed().isEmpty()) {
            if (!tokenEdit->text().trimmed().isEmpty()) {
                sendToken();
            } else {
                tokenEdit->setFocus();
            }
        }
    });

    connect(this, &LoginWidget::reset, this, [sendButton, userNameEdit, tokenEdit, this] {
        userNameEdit->clear();
        tokenEdit->clear();
        sendButton->setEnabled(false);
        m_messageLabel->clear();
    });
}

void LoginWidget::showMessage(const QString &message)
{
    m_messageLabel->setText(message);
}

void LoginWidget::hideMessage()
{
    m_messageLabel->clear();
}
