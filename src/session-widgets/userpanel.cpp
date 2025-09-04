// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userpanel.h"
#include "useravatar.h"

#include <DFontSizeManager>

#include <QHBoxLayout>

UserPanel::UserPanel(QWidget *parent)
    : ActionWidget(parent)
    , m_isSelected(false)
    , m_uid(UINT_MAX)
    , m_mainLayout(new QHBoxLayout(this))
    , m_avatar(new UserAvatar(this))
    , m_displayNameLabel(new DLabel(this))
    , m_typeLabel(new DLabel(this))
{
    setObjectName(QStringLiteral("UserPanel"));
    setAccessibleName(QStringLiteral("UserPanel"));

    setFixedSize(UserPanelWidth, UserPanelHeight);
    setFocusPolicy(Qt::NoFocus);
    initUI();
}

void UserPanel::initUI()
{
    setRadius(18);
    m_displayNameLabel->setAlignment(Qt::AlignLeft);
    DFontSizeManager::instance()->bind(m_displayNameLabel, DFontSizeManager::T5);
    m_displayNameLabel->setElideMode(Qt::ElideRight);

    m_typeLabel->setAlignment(Qt::AlignLeft);
    DFontSizeManager::instance()->bind(m_typeLabel, DFontSizeManager::T6);
    m_typeLabel->setElideMode(Qt::ElideRight);

    m_avatar->setAvatarSize(UserAvatarSize);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    QWidget *rightWidget = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(0);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    rightLayout->setContentsMargins(0, 0, 0, 0);
#else
    rightLayout->setMargin(0);
#endif
    rightLayout->addStretch();
    rightLayout->addWidget(m_displayNameLabel);
    rightLayout->addSpacing(6);
    rightLayout->addWidget(m_typeLabel);
    rightLayout->addStretch();
    m_mainLayout->addWidget(m_avatar);
    m_mainLayout->addWidget(rightWidget);
    m_mainLayout->addStretch();
    connect(m_avatar, &UserAvatar::clicked, this, &UserPanel::requestAction);
}

void UserPanel::setSelected(bool isSelected)
{
    m_isSelected = isSelected;
    setState(isSelected ? ActionWidget::Enter : ActionWidget::Leave);
    update();
}

/**
 * @brief 给当前 Widget 设置一个 id，用于排序
 * @param uid
 */
void UserPanel::setUid(const uint uid)
{
    m_uid = uid;
}

/**
 * @brief 设置用户头像
 * @param avatar
 */
void UserPanel::setAvatar(const QString &avatar)
{
    m_avatar->setIcon(avatar);
    update();
}

void UserPanel::setDisplayName(const QString &name)
{
    m_displayNameLabel->setText(name);
    update();
}

void UserPanel::setName(const QString &name)
{
    m_name = name;
}

void UserPanel::setFullName(const QString &name)
{
    m_fullName = name;
}

void UserPanel::setType(const QString &type)
{
    m_typeLabel->setText(type);
    update();
}
