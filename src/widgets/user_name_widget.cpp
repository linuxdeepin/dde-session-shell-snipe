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
    , m_userPic(nullptr)
    , m_userName(nullptr)
    , m_displayNameLabel(nullptr)
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

    m_userPic = new DLabel(this);
    m_userPic->setAlignment(Qt::AlignVCenter);
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/img/user_name.svg");
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_userPic->setAccessibleName(QStringLiteral("CapsStateLabel"));
    m_userPic->setPixmap(pixmap);
    m_userPic->setFixedSize(USER_PIC_HEIGHT, USER_PIC_HEIGHT);

    m_userName = new DLabel(this);
    m_userName->setAlignment(Qt::AlignVCenter);
    QPalette palette = m_userName->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_userName->setPalette(palette);
    DFontSizeManager::instance()->bind(m_userName, DFontSizeManager::T6);

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
    hLayout->addWidget(m_userPic);
    hLayout->addWidget(m_userName);
    hLayout->addStretch();

    setLayout(vLayout);

    const bool showUserName = DConfigHelper::instance()->getConfig(SHOW_USER_NAME, false).toBool();
    m_userName->setVisible(showUserName);
    m_userPic->setVisible(showUserName);
}

void UserNameWidget::updateUserName(const QString &userName)
{
    const int nameWidth = m_userName->fontMetrics().boundingRect(userName).width();
    const int labelMaxWidth = width() - 20;

    if (nameWidth > labelMaxWidth) {
        const QString &str = m_userName->fontMetrics().elidedText(userName, Qt::ElideRight, labelMaxWidth);
        m_userName->setText(str);
    } else {
        m_userName->setText(userName);
    }
}

void UserNameWidget::updateDisplayName(const QString &displayName)
{
    if (!m_displayNameLabel)
        return;

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
    if (m_userPic)
        height += USER_PIC_HEIGHT;
    if (m_userName)
        height += m_userName->fontMetrics().height();
    if (m_displayNameLabel)
        height += m_displayNameLabel->fontMetrics().height();

    return height;
}

void UserNameWidget::OnDConfigPropertyChanged(const QString &key, const QVariant &value)
{
    if (key == SHOW_USER_NAME) {
        const bool showUserName = value.toBool();
        m_userName->setVisible(showUserName);
        m_userPic->setVisible(showUserName);
    }
}
