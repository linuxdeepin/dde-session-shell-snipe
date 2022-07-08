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
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>

#include <DSysInfo>

#include "public_func.h"
#include "util_updateui.h"
#include "logowidget.h"

DCORE_USE_NAMESPACE

#define PIXMAP_WIDTH 128
#define PIXMAP_WIDTH_EXT 188
#define PIXMAP_HEIGHT 132 /* SessionBaseWindow */

const QPixmap systemLogo(const QString &file, const QSize &size, bool loadFromDTK)
{
    if (loadFromDTK) {
        return loadPixmap(DSysInfo::distributionOrgLogo(DSysInfo::Distribution, DSysInfo::Transparent, file), size);
    } else {
        return loadPixmap(file, size);
    }
}

LogoWidget::LogoWidget(QWidget *parent)
    : QFrame(parent)
    , m_licenseState(Unauthorized)
    , m_authorizationProperty(Default)
    , m_logoLabel(new QLabel(this))
    , m_logoVersionLabel(new DLabel(this))
{
    setObjectName("LogoWidget");
    m_logoLabel->setAccessibleName("LogoLabel");
    //设置QSizePolicy为固定高度,以保证右边版本号内容顶部能和图片对齐
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_locale = QLocale::system().name();

    initUI();

    QDBusConnection::systemBus().connect("com.deepin.license",
                                         "/com/deepin/license/Info",
                                         "com.deepin.license.Info",
                                         "LicenseStateChange",
                                         this, SLOT(updateLicenseState()));

    QDBusConnection::systemBus().connect("com.deepin.license",
                                         "/com/deepin/license/Info",
                                         "com.deepin.license.Info",
                                         "AuthorizationPropertyChange",
                                         this, SLOT(updateLicenseAuthorizationProperty()));

    updateLicenseState();
    updateLicenseAuthorizationProperty();
}

LogoWidget::~LogoWidget()
{
}

void LogoWidget::initUI()
{
    QHBoxLayout *logoLayout = new QHBoxLayout(this);
    logoLayout->setContentsMargins(48, 0, 0, 0);
    logoLayout->setSpacing(5);

    /* logo */
    m_logoLabel->setObjectName("Logo");
    updateLogoPixmap();
    logoLayout->addWidget(m_logoLabel, 0, Qt::AlignBottom | Qt::AlignLeft);

    /* version */
    m_logoVersionLabel->setObjectName("LogoVersion");
    //设置版本号显示部件自动拉伸,以保证高度和图标高度一致
    m_logoVersionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logoVersionLabel->setContentsMargins(0, m_logoLabel->geometry().y(), 0, 0);
    //设置版本号在左上角显示,以保证内容顶部和图标顶部对齐
    m_logoVersionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
#ifdef SHENWEI_PLATFORM
    QPalette pe;
    pe.setColor(QPalette::WindowText, Qt::white);
    m_logoVersionLabel->setPalette(pe);
#endif
    //由于有些字体不支持一些语言，会造成切换语言后登录界面和锁屏界面版本信息大小不一致
    //设置版本信息的默认字体为系统的默认字体以便于支持更多语言
    QFont font(m_logoVersionLabel->font());
    font.setFamily("Sans Serif");
    font.setPixelSize(m_logoLabel->height() / 2);
    m_logoVersionLabel->setFont(font);

    if (DSysInfo::UosEdition::UosEducation != DSysInfo::uosEditionType()) {  //教育版登录界面不要显示系统版本号（和Logo冲突）
        logoLayout->addWidget(m_logoVersionLabel);;
    }

    updateStyle(":/skin/login.qss", m_logoVersionLabel);
}

QPixmap LogoWidget::loadSystemLogo(const QString &file, bool loadFromDTK)
{
    QPixmap p = systemLogo(file, QSize(), loadFromDTK);

    QSize size(PIXMAP_WIDTH_EXT, PIXMAP_HEIGHT);
    if (loadFromDTK) {
        size.setWidth(PIXMAP_WIDTH);
        size.setHeight(PIXMAP_HEIGHT);
    }

    const bool result = p.width() < size.width() && p.height() < size.height();
    return result ? p : systemLogo(file, size, loadFromDTK);
}

QString LogoWidget::getVersion()
{
    QString version;

    if (DSysInfo::uosType() == DSysInfo::UosServer) {
        version = QString("%1").arg(DSysInfo::majorVersion());
    } else if (DSysInfo::isDeepin()) {
        // 获取系统版本，例如20专业版
        version = QString("%1 %2").arg(DSysInfo::majorVersion()).arg(DSysInfo::uosEditionName(m_locale));

        // 如果已经授权，并且是政务授权或企业授权，则不显示文案
        if (m_licenseState == Authorized &&
            (m_authorizationProperty == AuthorizationProperty::Government ||
             m_authorizationProperty == AuthorizationProperty::Enterprise)) {
                version = "";
        }
    } else {
        version = QString("%1 %2").arg(DSysInfo::productVersion()).arg(DSysInfo::productTypeString());
    }

    return version;
}

void LogoWidget::updateLogoPixmap()
{
    QString logo = ":img/logo.svg";
    bool loadFromDTK = true;

    // 专业版需要区分授权类型
    if (DSysInfo::isDeepin()) {
        if (m_licenseState == Authorized) {
            // 根据用户设置的系统语言显示LOGO,简体、繁体、正体中文和藏语、维语均显示中文，其他显示英文
            QString lang = "-EN";
            if (m_locale.contains("zh_") ||
                m_locale.contains("ug_") ||
                m_locale.contains("bo_")) {
                lang = "-ZH";
            }

            // 根据授权类型不同显示不同的LOGO图片，其他按原来的方式显示
            if (m_authorizationProperty == Enterprise) {
                logo =  QString(":img/enterprise%1.svg").arg(lang);
                loadFromDTK = false;
            } else if (m_authorizationProperty == Government) {
                logo =  QString(":img/government%1.svg").arg(lang);
                loadFromDTK = false;
            }
        }
    }

    QPixmap pixmap = loadSystemLogo(logo, loadFromDTK);
    m_logoLabel->setPixmap(pixmap);

    m_logoVersionLabel->setText(getVersion());
}

/**
 * @brief LogoWidget::updateLocale
 * 将翻译文件与用户选择的语言对应
 * @param locale
 */
void LogoWidget::updateLocale(const QString &locale)
{
    m_locale = locale;
    updateLogoPixmap();
}

void LogoWidget::updateLicenseState()
{
    QDBusInterface licenseInfo("com.deepin.license",
                               "/com/deepin/license/Info",
                               "org.freedesktop.DBus.Properties",
                               QDBusConnection::systemBus());

    QDBusPendingCall call = licenseInfo.asyncCall("Get",  "com.deepin.license.Info", "LicenseState");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, call, watcher] {
        if (!call.isError()) {
            QDBusReply<QDBusVariant> reply = call.reply();
            m_licenseState = reply.value().variant().toInt();
            updateLogoPixmap();
            update();
        } else {
            qDebug() << "Failed to get license state: " << call.error().message();
        }
        watcher->deleteLater();
    });
}

void LogoWidget::updateLicenseAuthorizationProperty()
{
    QDBusInterface licenseInfo("com.deepin.license",
                               "/com/deepin/license/Info",
                               "org.freedesktop.DBus.Properties",
                               QDBusConnection::systemBus());

    QDBusPendingCall call = licenseInfo.asyncCall("Get",  "com.deepin.license.Info", "AuthorizationProperty");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, call, watcher] {
        if (!call.isError()) {
            QDBusReply<QDBusVariant> reply = call.reply();
            m_authorizationProperty = reply.value().variant().toInt();
            updateLogoPixmap();
            update();
        } else {
            qDebug() << "Failed to get authorization property: " << call.error().message();
        }
        watcher->deleteLater();
    });
}

void LogoWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    m_logoVersionLabel->setContentsMargins(0, m_logoLabel->geometry().y(), 0, 0);
    /** TODO
     * 使用系统名称图标一半高度做为版本信息字体大小。
     * 当系统名称图标比较大时，版本信息也会比较大，这里有待优化。
     */
    QFont font(m_logoVersionLabel->font());
    font.setPixelSize(m_logoLabel->height() / 2);
    m_logoVersionLabel->setFont(font);
}
