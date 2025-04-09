// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "udcp_mfa_login_widget.h"

#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QFile>
#include <QRegExp>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>

static void set_style(QWidget *widget, const QString &path)
{
    QFile file(path);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    file.close();
    widget->setStyleSheet(content);
}

UdcpMFALoginWidget::UdcpMFALoginWidget(const QString &user, QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_phoneEdit(new QLineEdit("1592****994", this))
    , m_sendButton(new QPushButton(tr("发送验证码"), this))
    , m_codeEdit(new QLineEdit(this))
    , m_verifyButton(new QPushButton(QIcon(":/images/next1.png"), "", m_codeEdit))
{
    set_style(this, ":/qss/udcp_mfa_login_widget.qss");
    initUI();
    initConnections();
}
void UdcpMFALoginWidget::initUI()
{
    m_phoneEdit->setObjectName("phoneEdit");
    m_phoneEdit->setContextMenuPolicy(Qt::NoContextMenu);
    if (m_phoneEdit->text().isEmpty()) {
        m_phoneEdit->setPlaceholderText(tr("请输入手机号"));
        QRegularExpression phoneRegExp("^1[3-9]\\d{9}$");
        m_phoneEdit->setValidator(new QRegularExpressionValidator(phoneRegExp, this));
    } else {
        m_phoneEdit->setDisabled(true);
    }

    m_sendButton->setObjectName("sendButton");
    m_sendButton->setEnabled(!m_phoneEdit->isEnabled());
    m_sendButton->setFixedWidth(100);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(m_phoneEdit);
    topLayout->addWidget(m_sendButton);

    m_codeEdit->setTextMargins(0, 0, 30, 0);
    m_codeEdit->setMinimumWidth(280);
    m_codeEdit->setPlaceholderText(tr("请输入验证码"));
    m_codeEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_codeEdit->setObjectName("codeEdit");
    QRegularExpression codeRegExp("^\\d{4}$");
    m_codeEdit->setValidator(new QRegularExpressionValidator(codeRegExp, this));

    m_verifyButton->setObjectName("verifyButton");
    m_verifyButton->setCursor(Qt::ArrowCursor);
    m_verifyButton->setContentsMargins(0, 0, 0, 0);
    m_verifyButton->setEnabled(false);

    auto codeLayout = new QHBoxLayout(m_codeEdit);
    codeLayout->setContentsMargins(8, 0, 8, 0);
    codeLayout->addStretch(1);
    codeLayout->addWidget(m_verifyButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    auto *layout = new QVBoxLayout;
    layout->addLayout(topLayout);
    layout->addWidget(m_codeEdit);

    setLayout(layout);

    if (m_phoneEdit->isEnabled()) {
        setFocusProxy(m_phoneEdit);
    } else {
        setFocusProxy(m_codeEdit);
    }
}

void UdcpMFALoginWidget::initConnections()
{
    if (m_phoneEdit->isEnabled()) {
        connect(m_phoneEdit, &QLineEdit::textChanged, [this] {
            if (m_phoneEdit->hasAcceptableInput() && m_timer->isActive()) {
                return;
            }
            m_sendButton->setEnabled(m_phoneEdit->hasAcceptableInput());
        });
    }

    connect(m_timer, &QTimer::timeout, [this] {
        if (--m_count <= 0) {
            m_sendButton->setText(tr("获取验证码"));
            m_sendButton->setEnabled(m_phoneEdit->hasAcceptableInput());
            m_timer->stop();
        } else {
            m_sendButton->setText(QString("%1s").arg(m_count));
        }
    });

    connect(m_sendButton, &QPushButton::clicked, [this] {
        // 按钮禁用，按钮显示倒计时60s，60s后按钮启用
        m_sendButton->setEnabled(false);
        m_timer->setInterval(1000);
        m_count = 60;
        m_sendButton->setText(QString("%1s").arg(m_count));
        m_timer->start();
    });

    connect(m_verifyButton, &QPushButton::pressed, [this] {
        m_verifyButton->setIcon(QIcon(":/images/next2.png"));
    });
    connect(m_verifyButton, &QPushButton::released, [this] {
        m_verifyButton->setIcon(QIcon(":/images/next1.png"));
    });
    connect(m_codeEdit, &QLineEdit::textChanged, [this] {
        m_verifyButton->setEnabled(m_codeEdit->hasAcceptableInput());
    });
    connect(m_codeEdit, &QLineEdit::returnPressed, [this] {
        if (m_phoneEdit->hasAcceptableInput()) {
            m_verifyButton->click();
        }
    });
    connect(m_verifyButton, &QPushButton::clicked, [this] {
        Q_EMIT finished();
    });
}


void UdcpMFALoginWidget::reset()
{
    m_count = 0;
    m_timer->stop();
    m_phoneEdit->setText("1592****994");
    m_codeEdit->clear();
    m_phoneEdit->disconnect();
    m_phoneEdit->setEnabled(m_phoneEdit->text().isEmpty());
    m_sendButton->setText(tr("获取验证码"));
    m_sendButton->setEnabled(!m_phoneEdit->isEnabled());
    if (m_phoneEdit->isEnabled()) {
        m_phoneEdit->setPlaceholderText(tr("请输入手机号"));
        QRegularExpression phoneRegExp("^1[3-9]\\d{9}$");
        m_phoneEdit->setValidator(new QRegularExpressionValidator(phoneRegExp, this));
        connect(m_phoneEdit, &QLineEdit::textChanged, [this] {
            if (m_phoneEdit->hasAcceptableInput() && m_timer->isActive()) {
                return;
            }
            m_sendButton->setEnabled(m_phoneEdit->hasAcceptableInput());
        });
        setFocusProxy(m_phoneEdit);
    } else {
        setFocusProxy(m_codeEdit);
    }
}
