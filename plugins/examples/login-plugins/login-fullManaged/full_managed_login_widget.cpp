// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_managed_login_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

FullManagedLoginWidget::FullManagedLoginWidget(QWidget *parent)
    : QWidget(parent)
    , m_messageLabel(nullptr)
    , m_userNameLabel(nullptr)
    , m_userNameEdit(nullptr)
    , m_tokenLabel(nullptr)
    , m_tokenEdit(nullptr)
    , m_sendButton(nullptr)
    , m_disMissButton(nullptr)
{
    setObjectName(QStringLiteral("LoginWidget"));
    setAccessibleName(QStringLiteral("LoginWidget"));

    initUI();
    initConnection();
}

void FullManagedLoginWidget::initUI()
{
    // init ui
    m_userNameLabel = new QLabel("User Name: ", this);
    m_userNameLabel->setFixedHeight(35);

    m_userNameEdit = new QLineEdit(this);
    m_userNameEdit->setFixedHeight(35);

    QHBoxLayout *m_userNameLayout = new QHBoxLayout();
    m_userNameLayout->addWidget(m_userNameLabel, 3, Qt::AlignRight);
    m_userNameLayout->addSpacing(15);
    m_userNameLayout->addWidget(m_userNameEdit, 7);
    setFocusProxy(m_userNameEdit);

    m_tokenLabel = new QLabel("Token: ", this);
    m_tokenLabel->setFixedHeight(35);

    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setFixedHeight(35);

    QHBoxLayout *tokenLayout = new QHBoxLayout();
    tokenLayout->addWidget(m_tokenLabel, 3, Qt::AlignRight);
    tokenLayout->addSpacing(15);
    tokenLayout->addWidget(m_tokenEdit, 7);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setMinimumHeight(80);
    m_messageLabel->setWordWrap(true);
    QFont font = m_messageLabel->font();
    font.setPixelSize(30);
    m_messageLabel->setFont(font);
    m_messageLabel->setAlignment(Qt::AlignCenter);

    m_sendButton = new QPushButton("Send Token", this);
    m_sendButton->setFixedSize(120, 35);
    m_sendButton->setEnabled(false);
    m_sendButton->setDefault(true);

    m_disMissButton = new QPushButton("deactive", this);
    m_disMissButton->setFixedSize(120, 35);
    m_disMissButton->setEnabled(true);
    m_disMissButton->setDefault(true);

    QVBoxLayout *m_layout = new QVBoxLayout(this);
    setLayout(m_layout);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_layout->setContentsMargins(15, 15, 15, 15);
#else
    m_layout->setMargin(15);
#endif
    m_layout->addLayout(m_userNameLayout);
    m_layout->addSpacing(30);
    m_layout->addLayout(tokenLayout);
    m_layout->addSpacing(50);
    m_layout->addWidget(m_messageLabel);
    m_layout->addStretch(1);
    m_layout->addWidget(m_sendButton, 0, Qt::AlignCenter);
    m_layout->addWidget(m_disMissButton, 0, Qt::AlignCenter);
    m_layout->addSpacing(15);
}

void FullManagedLoginWidget::initConnection()
{
    // init connections
    auto setButtonEnabledFunc = [this] {
        const QString &userName = m_userNameEdit->text().trimmed();
        const QString &token = m_tokenEdit->text().trimmed();
        m_sendButton->setEnabled(!userName.isEmpty() && !token.isEmpty());
    };

    auto sendTokenFunc = [this] {
        const QString &userName = m_userNameEdit->text().trimmed();
        const QString &token = m_tokenEdit->text().trimmed();
        if (!userName.isEmpty() && !token.isEmpty()) {
            Q_EMIT sendAuthToken(userName, token);
        }
    };

    connect(m_userNameEdit, &QLineEdit::textChanged, setButtonEnabledFunc);
    connect(m_tokenEdit, &QLineEdit::textChanged, setButtonEnabledFunc);

    connect(m_sendButton, &QPushButton::clicked, this, sendTokenFunc);
    connect(this, &FullManagedLoginWidget::greeterAuthStarted, this, sendTokenFunc);

    connect(m_disMissButton, &QPushButton::clicked, this, [this] {
        Q_EMIT deactived();
        m_disMissButton->setDisabled(true);
        QTimer::singleShot(DEACTIVE_TIME_INTERVAL, this, [this] {
            m_disMissButton->setDisabled(false);
        });
    });

    connect(m_tokenEdit, &QLineEdit::returnPressed, this, sendTokenFunc);
    connect(m_userNameEdit, &QLineEdit::returnPressed, this, [this] {
        if (!m_userNameEdit->text().trimmed().isEmpty()) {
            if (!m_tokenEdit->text().trimmed().isEmpty()) {
                const QString &userName = m_userNameEdit->text().trimmed();
                const QString &token = m_tokenEdit->text().trimmed();

                Q_EMIT sendAuthToken(userName, token);
            } else {
                m_tokenEdit->setFocus();
            }
        }
    });

    connect(this, &FullManagedLoginWidget::reset, this, [this] {
        m_userNameEdit->clear();
        m_tokenEdit->clear();
        m_sendButton->setEnabled(false);
        m_messageLabel->clear();
    });
}

void FullManagedLoginWidget::showMessage(const QString &message)
{
    m_messageLabel->setText(message);
}

void FullManagedLoginWidget::hideMessage()
{
    m_messageLabel->clear();
}
