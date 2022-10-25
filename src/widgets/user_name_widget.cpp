// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "user_name_widget.h"
#include "public_func.h"
#include "dconfig_helper.h"
#include "constants.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>

#include <DHiDPIHelper>
#include <DFontSizeManager>
#include <qpalette.h>

const int USER_PIC_HEIGHT = 16;

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE
using namespace DDESESSIONCC;

UserNameWidget::UserNameWidget(bool showDisplayName, QWidget *parent)
    : QWidget(parent)
    , m_userPicLabel(nullptr)
    , m_fullNameLabel(nullptr)
    , m_displayNameLabel(nullptr)
    , m_showUserName(false)
    , m_showDisplayName(showDisplayName)
{
    initialize();
    DConfigHelper::instance()->bind(this, SHOW_USER_NAME);
}

void UserNameWidget::initialize()
{
    setAccessibleName("UserNameWidget");
    QVBoxLayout *vLayout = new QVBoxLayout;
    QHBoxLayout *hLayout = new QHBoxLayout;

    m_userPicLabel = new DLabel(this);
    m_userPicLabel->setAlignment(Qt::AlignVCenter);
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/img/user_name.svg");
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_userPicLabel->setAccessibleName(QStringLiteral("CapsStateLabel"));
    m_userPicLabel->setPixmap(pixmap);
    m_userPicLabel->setFixedSize(USER_PIC_HEIGHT, USER_PIC_HEIGHT);

    m_fullNameLabel = new DLabel(this);
    m_fullNameLabel->setAlignment(Qt::AlignVCenter);
    QPalette palette = m_fullNameLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_fullNameLabel->setPalette(palette);
    DFontSizeManager::instance()->bind(m_fullNameLabel, DFontSizeManager::T6);

    if (m_showDisplayName) {
        m_displayNameLabel = new DLabel(this);
        m_displayNameLabel->setAccessibleName(QStringLiteral("NameLabel"));
        m_displayNameLabel->setTextFormat(Qt::TextFormat::PlainText);
        m_displayNameLabel->setAlignment(Qt::AlignCenter);
        m_displayNameLabel->setTextFormat(Qt::TextFormat::PlainText);
        DFontSizeManager::instance()->bind(m_displayNameLabel, DFontSizeManager::T2);
        palette = m_displayNameLabel->palette();
        palette.setColor(QPalette::WindowText, Qt::white);
        m_displayNameLabel->setPalette(palette);
        vLayout->addWidget(m_displayNameLabel);
    }

    vLayout->addLayout(hLayout);
    hLayout->addStretch();
    hLayout->addWidget(m_userPicLabel);
    hLayout->addWidget(m_fullNameLabel);
    hLayout->addStretch();

    setLayout(vLayout);

    m_showUserName = DConfigHelper::instance()->getConfig(SHOW_USER_NAME, false).toBool();
    m_fullNameLabel->setVisible(m_showUserName);
    m_userPicLabel->setVisible(m_showUserName);
}

void UserNameWidget::updateUserName(const QString &userName)
{
    if (userName == m_userNameStr)
        return;

    m_userNameStr = userName;
    updateUserNameWidget();
    updateDisplayNameWidget();
}

void UserNameWidget::updateUserNameWidget()
{
    const int nameWidth = m_fullNameLabel->fontMetrics().boundingRect(m_fullNameStr).width();
    const int labelMaxWidth = width() - 20;

    if (nameWidth > labelMaxWidth) {
        const QString &str = m_fullNameLabel->fontMetrics().elidedText(m_fullNameStr, Qt::ElideRight, labelMaxWidth);
        m_fullNameLabel->setText(str);
    } else {
        m_fullNameLabel->setText(m_fullNameStr);
    }
}

void UserNameWidget::updateFullName(const QString &fullName)
{
    if (fullName == m_fullNameStr)
        return;

    m_fullNameStr = fullName;
    updateUserNameWidget();
    updateDisplayNameWidget();
}

/**
 * @brief 如果在DConfig中配置了showUserName为true，那么displayName显示用户名，
 * 否则显示全名，如果全名为空，则显示用户名
 *
 */
void UserNameWidget::updateDisplayNameWidget()
{
    if (!m_displayNameLabel)
        return;

    QString displayName = m_showUserName ? m_userNameStr : (m_fullNameStr.isEmpty() ? m_userNameStr : m_fullNameStr);
    int nameWidth = m_displayNameLabel->fontMetrics().boundingRect(displayName).width();
    int labelMaxWidth = width() - 20;

    if (nameWidth > labelMaxWidth) {
        QString str = m_displayNameLabel->fontMetrics().elidedText(displayName, Qt::ElideRight, labelMaxWidth);
        m_displayNameLabel->setText(str);
    } else {
        m_displayNameLabel->setText(displayName);
    }
}

int UserNameWidget::heightHint() const
{
    int height = 0;
    if (m_userPicLabel)
        height += USER_PIC_HEIGHT;
    if (m_fullNameLabel)
        height += m_fullNameLabel->fontMetrics().height();
    if (m_displayNameLabel)
        height += m_displayNameLabel->fontMetrics().height();

    return height;
}

void UserNameWidget::OnDConfigPropertyChanged(const QString &key, const QVariant &value)
{
    if (key == SHOW_USER_NAME) {
        m_showUserName = value.toBool();
        m_fullNameLabel->setVisible(m_showUserName);
        m_userPicLabel->setVisible(m_showUserName);
        updateUserNameWidget();
        updateDisplayNameWidget();
    }
}
