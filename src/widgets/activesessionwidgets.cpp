// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "activesessionwidgets.h"
#include "statebutton.h"
#include "useravatar.h"
#include "userinfo.h"

#include <DFontSizeManager>
#include <DSpinner>

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace {
constexpr int kAvatarSize = 100;
}

ActiveSessionConfirmWidget::ActiveSessionConfirmWidget(QWidget *parent)
    : QWidget(parent)
    , m_avatar(new UserAvatar(this))
    , m_userNameLabel(new QLabel(this))
    , m_messageLabel(new QLabel(this))
    , m_confirmButton(new StateButton(tr("Confirm Login"), this))
    , m_cancelButton(new StateButton(tr("Cancel"), this))
{
    initUI();
}

void ActiveSessionConfirmWidget::setUser(const std::shared_ptr<User> &user)
{
    m_user = user;
    if (m_user) {
        m_userNameLabel->setText(m_user->displayName());
    } else {
        m_userNameLabel->clear();
    }
    updateAvatar();
}

void ActiveSessionConfirmWidget::initUI()
{
    setAccessibleName("ActiveSessionConfirmWidget");

    m_avatar->setAvatarSize(kAvatarSize);
    m_avatar->setObjectName("Avatar_4");

    m_userNameLabel->setAlignment(Qt::AlignCenter);
    DFontSizeManager::instance()->bind(m_userNameLabel, DFontSizeManager::T2, QFont::Light);

    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    DFontSizeManager::instance()->bind(m_messageLabel, DFontSizeManager::T7);
    m_messageLabel->setMinimumWidth(550);

    m_cancelButton->setObjectName("ActiveSessionCancelButton");
    m_cancelButton->setFixedHeight(42);
    m_cancelButton->setMinimumWidth(130);
    m_cancelButton->setBorderRadius(8);
    m_cancelButton->setNormalTextColor(QColor(255, 255, 255));
    m_cancelButton->setHoverTextColor(QColor(0, 0, 0));
    m_cancelButton->setPressTextColor(QColor(0, 129, 255));
    m_cancelButton->setNormalBackground(QColor(255, 255, 255, 25));
    m_cancelButton->setHoverBackground(QColor(255, 255, 255, 102));
    m_cancelButton->setPressBackground(QColor(255, 255, 255, 51));
    DFontSizeManager::instance()->bind(m_cancelButton, DFontSizeManager::T7);

    m_confirmButton->setObjectName("ActiveSessionConfirmButton");
    m_confirmButton->setFixedHeight(42);
    m_confirmButton->setMinimumWidth(130);
    m_confirmButton->setBorderRadius(8);
    m_confirmButton->setNormalTextColor(QColor(255, 255, 255));
    m_confirmButton->setHoverTextColor(QColor(0, 0, 0));
    m_confirmButton->setPressTextColor(QColor(0, 129, 255));
    m_confirmButton->setNormalBackground(QColor(255, 255, 255, 25));
    m_confirmButton->setHoverBackground(QColor(255, 255, 255, 102));
    m_confirmButton->setPressBackground(QColor(255, 255, 255, 51));
    DFontSizeManager::instance()->bind(m_confirmButton, DFontSizeManager::T7);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(60);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_confirmButton);
    buttonLayout->addStretch();

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->addStretch();
    mainLayout->addWidget(m_avatar, 0, Qt::AlignHCenter);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_userNameLabel, 0, Qt::AlignHCenter);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(m_messageLabel, 0, Qt::AlignHCenter);
    mainLayout->addSpacing(30);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
    setLayout(mainLayout);

    connect(m_confirmButton, &QPushButton::clicked, this, &ActiveSessionConfirmWidget::confirmed);
    connect(m_cancelButton, &QPushButton::clicked, this, &ActiveSessionConfirmWidget::canceled);
}

void ActiveSessionConfirmWidget::setLimitMode(bool limitMode)
{
    if (limitMode) {
        m_messageLabel->setText(tr("Two remote users are already signed in, which is the maximum allowed. Wait for a remote user to sign out, then try again."));
        m_confirmButton->hide();
    } else {
        m_messageLabel->setText(tr("This account is already logged in elsewhere. Continuing will force logout the existing session. Proceed with login?"));
        m_confirmButton->show();
    }
}

void ActiveSessionConfirmWidget::updateAvatar()
{
    if (m_user && !m_user->avatar().isEmpty()) {
        m_avatar->setIcon(m_user->avatar());
    }
}

ActiveSessionLoadingWidget::ActiveSessionLoadingWidget(QWidget *parent)
    : QWidget(parent)
    , m_spinner(new Dtk::Widget::DSpinner(this))
{
    initUI();
    m_spinner->start();
}

void ActiveSessionLoadingWidget::initUI()
{
    setAccessibleName("ActiveSessionLoadingWidget");

    m_spinner->setFixedSize(56, 56);
    QPalette pa(m_spinner->palette());
    pa.setColor(QPalette::Highlight, Qt::white);
    m_spinner->setPalette(pa);

    auto *rowLayout = new QHBoxLayout;
    rowLayout->addStretch();
    rowLayout->addWidget(m_spinner);
    rowLayout->addStretch();

    setLayout(rowLayout);
}
