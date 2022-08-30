// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logintipswindow.h"
#include "constants.h"

#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPalette>
#include <QFile>
#include <QTextStream>

DWIDGET_USE_NAMESPACE
LoginTipsWindow::LoginTipsWindow(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void LoginTipsWindow::initUI()
{
    setAccessibleName("LoginTipsWindow");
    m_mainLayout = new QHBoxLayout(this);

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    // 提示内容布局
    m_content = new QLabel();
    m_content->setAccessibleName("ContentLabel");
    m_content->setWordWrap(true);
    QPalette t_palette = m_content->palette();
    t_palette.setColor(QPalette::WindowText, Qt::white);
    m_content->setPalette(t_palette);
    m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    DFontSizeManager::instance()->bind(m_content, DFontSizeManager::T4);
    m_content->setAlignment(Qt::AlignCenter);
    m_content->setTextFormat(Qt::TextFormat::PlainText);

    // 获取/usr/share/dde-session-shell/dde-session-shell.conf 配置信息
    m_contentString = findValueByQSettings<QString>(DDESESSIONCC::session_ui_configs, "Greeter", "tipsContent", "");
    m_content->setText(m_contentString);

    // 提示标题布局
    m_tipLabel = new QLabel();
    m_tipLabel->setAccessibleName("TipLabel");
    m_tipLabel->setAlignment(Qt::AlignHCenter);
    QPalette palette = m_tipLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_tipLabel->setPalette(palette);
    m_tipLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont font = m_tipLabel->font();
    font.setBold(true);
    m_tipLabel->setFont(font);
    DFontSizeManager::instance()->bind(m_tipLabel, DFontSizeManager::T2, QFont::DemiBold);
    m_tipLabel->setAlignment(Qt::AlignCenter);
    m_tipLabel->setTextFormat(Qt::TextFormat::PlainText);

    // 获取/usr/share/dde-session-shell/dde-session-shell.conf 配置信息
    m_tipString = findValueByQSettings<QString>(DDESESSIONCC::session_ui_configs, "Greeter", "tipsTitle", "");
    m_tipLabel->setText(m_tipString);

    // 确认按钮
    m_btn = new QPushButton();
    m_btn->setFixedSize(90, 40);
    m_btn->setObjectName("RequireSureButton");
    m_btn->setText("OK");

    vLayout->addStretch();
    vLayout->addWidget(m_tipLabel, 0, Qt::AlignHCenter);
    vLayout->addWidget(m_content, 0, Qt::AlignHCenter);
    vLayout->addWidget(m_btn, 0, Qt::AlignHCenter);
    vLayout->addStretch();

    m_mainLayout->addStretch();
    m_mainLayout->addLayout(vLayout);
    m_mainLayout->addStretch();
    setLayout(m_mainLayout);

    connect(m_btn, &QPushButton::clicked, this, [=] {
        // 点击确认后打开登录界面
        emit requestClosed();
        this->close();
    });
}

bool LoginTipsWindow::isValid()
{
    return (!m_tipString.isEmpty() || !m_contentString.isEmpty());
}

