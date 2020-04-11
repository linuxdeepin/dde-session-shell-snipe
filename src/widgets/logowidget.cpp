/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QTextCodec>
#include <QPalette>
#include <QDebug>
#include <QSettings>
#include <DSysInfo>
#include "src/global_util/public_func.h"
#include "src/global_util/util_updateui.h"

#include "logowidget.h"

DCORE_USE_NAMESPACE

const QPixmap systemLogo()
{
    return loadPixmap(DSysInfo::distributionOrgLogo(DSysInfo::Distribution, DSysInfo::Transparent, ":img/logo.svg"));
}

LogoWidget::LogoWidget(QWidget* parent)
    : QFrame(parent)
{
    m_locale = QLocale::system().name();
    initUI();
}

void LogoWidget::initUI() {
//    setFixedSize(240, 40);

    m_logoLabel = new QLabel();
    m_logoLabel->setPixmap(systemLogo());
    m_logoLabel->setObjectName("Logo");
    m_logoLabel->setFixedSize(128, 48);
    //修复社区版deepin的显示不全的问题 2020/04/11
    m_logoLabel->setScaledContents(true);

    m_logoVersionLabel = new QLabel;
    m_logoVersionLabel->setObjectName("LogoVersion");
#ifdef SHENWEI_PLATFORM
    QPalette pe;
    pe.setColor(QPalette::WindowText,Qt::white);
    m_logoVersionLabel->setPalette(pe);
#endif
    this->setObjectName("LogoWidget");

    m_logoLayout = new QHBoxLayout;
    m_logoLayout->setMargin(0);
    m_logoLayout->setSpacing(0);
    m_logoLayout->addSpacing(48);
    m_logoLayout->addWidget(m_logoLabel);
    m_logoLayout->addWidget(m_logoVersionLabel, 0, Qt::AlignTop);

    setLayout(m_logoLayout);

    QString systemVersion = getVersion();
    m_logoVersionLabel->setText(systemVersion);
    adjustSize();

    updateStyle(":/skin/login.qss", m_logoVersionLabel);
}

QString LogoWidget::getVersion() {
    QSettings settings("/etc/deepin-version", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("utf8"));
    QString item = "Release";
    ///////////system version
    QString version = settings.value(item + "/Version").toString();
    //////////system type
    QString localKey =QString("%1/Type[%2]").arg(item).arg(m_locale);
    QString finalKey =QString("%1/Type").arg(item);

    QString type = settings.value(localKey, settings.value(finalKey)).toString();
    //////////system release version
    QString milestone = settings.value("Addition/Milestone").toString();

    qDebug() << "Deepin Version:" << version << type;

//    return QString("%1 %2 %3").arg(version).arg(type).arg(milestone);
    return version;
}

LogoWidget::~LogoWidget()
{
}

void LogoWidget::updateLocale(const QString &locale)
{
    m_locale = locale;
    m_logoVersionLabel->setText(getVersion());
    adjustSize();
}
