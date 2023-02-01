/*
* Copyright (C) 2021 ~ 2022 Uniontech Software Technology Co.,Ltd.
*
* Author:     fanpengcheng <fanpengcheng@uniontech.com>
*
* Maintainer: fanpengcheng <fanpengcheng@uniontech.com>
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

#include "passwordlevelwidget.h"

#include <QHBoxLayout>

#include <DLabel>


#define IMG_PREFIX (QString(":/src/widgets/img/password_level/"))
PasswordLevelWidget::PasswordLevelWidget(QWidget *parent)
    : QWidget(parent)
    , m_level(PASSWORD_STRENGTH_LEVEL_ERROR)
    , m_layout(new QHBoxLayout(this))
    , m_tips(new DLabel(tr(""), this))
    , m_lowIcon(new DLabel(this))
    , m_mediumIcon(new DLabel(this))
    , m_highIcon(new DLabel(this))
{
    initUI();
    initConnections();

#ifndef QT_DEBUG
    // TODO 高度超出可显示的区域，导致显示异常，暂时屏蔽，具体的显示方式还在确认
    m_tips->setVisible(false);
#endif
}

void PasswordLevelWidget::reset()
{
    m_level = PASSWORD_STRENGTH_LEVEL_ERROR;

    m_tips->clear();
    m_lowIcon->setPixmap(defaultIcon());
    m_mediumIcon->setPixmap(defaultIcon());
    m_highIcon->setPixmap(defaultIcon());
}

void PasswordLevelWidget::setLevel(PASSWORD_LEVEL_TYPE level)
{
    if (m_level == level)
        return;

    m_level = level;

    QPalette palette = m_tips->palette();
    switch (level){
    case PASSWORD_STRENGTH_LEVEL_LOW:
    {
        palette.setColor(QPalette::WindowText, QColor("#FF5736"));
        m_tips->setPalette(palette);
        m_tips->setText(tr("Weak"));

        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_low.svg"));
        m_mediumIcon->setPixmap(defaultIcon());
        m_highIcon->setPixmap(defaultIcon());
    }
        break;
    case PASSWORD_STRENGTH_LEVEL_MIDDLE:
    {
        palette.setColor(QPalette::WindowText, QColor("#FFAA00"));
        m_tips->setPalette(palette);
        m_tips->setText(tr("Medium"));

        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_middle.svg"));
        m_mediumIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_middle.svg"));
        m_highIcon->setPixmap(defaultIcon());
    }
        break;
    case PASSWORD_STRENGTH_LEVEL_HIGH:
    {
        palette.setColor(QPalette::WindowText, QColor("#15BB18"));
        m_tips->setPalette(palette);
        m_tips->setText(tr("Strong"));
        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
        m_mediumIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
        m_highIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
    }
        break;
    default:
        reset();
        break;
    }
}

void PasswordLevelWidget::initUI()
{
    m_tips->clear();
    m_lowIcon->setPixmap(defaultIcon());
    m_mediumIcon->setPixmap(defaultIcon());
    m_highIcon->setPixmap(defaultIcon());

    m_layout->addWidget(m_tips);
    m_layout->addWidget(m_lowIcon);
    m_layout->addWidget(m_mediumIcon);
    m_layout->addWidget(m_highIcon);

    setLayout(m_layout);
}

void PasswordLevelWidget::initConnections()
{
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &PasswordLevelWidget::onThemeTypeChanged);
}

QString PasswordLevelWidget::defaultIcon()
{
    if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType) {
        return IMG_PREFIX + "dcc_deepin_password_strength_unactive_light_mode.svg";
    } else if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType) {
        return IMG_PREFIX + "dcc_deepin_password_strength_unactive_deep_mode.svg";
    } else {
        return QString();
    }
}

void PasswordLevelWidget::onThemeTypeChanged(DGuiApplicationHelper::ColorType type)
{
    Q_UNUSED(type)

    switch (m_level) {
    case PASSWORD_STRENGTH_LEVEL_LOW:
    {
        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_low.svg"));
        m_mediumIcon->setPixmap(defaultIcon());
        m_highIcon->setPixmap(defaultIcon());
    }
        break;
    case PASSWORD_STRENGTH_LEVEL_MIDDLE:
    {
        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_middle.svg"));
        m_mediumIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_middle.svg"));
        m_highIcon->setPixmap(defaultIcon());
    }
        break;
    case PASSWORD_STRENGTH_LEVEL_HIGH:
    {
        m_lowIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
        m_mediumIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
        m_highIcon->setPixmap(QPixmap(IMG_PREFIX + "dcc_deepin_password_strength_high.svg"));
    }
        break;
    default:
        reset();
        break;
    }
}
