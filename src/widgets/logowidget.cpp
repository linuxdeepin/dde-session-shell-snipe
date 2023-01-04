// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QObject>
#include <QtCore/QFile>
#include <QTextCodec>
#include <QPalette>
#include <QDebug>
#include <QSettings>
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
    , m_logoLabel(new QLabel(this))
    , m_logoVersionLabel(new DLabel(this))
{
    setObjectName("LogoWidget");
    m_logoLabel->setAccessibleName("LogoLabel");
    //设置QSizePolicy为固定高度,以保证右边版本号内容顶部能和图片对齐
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_locale = QLocale::system().name();

    initUI();
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

    if(DSysInfo::UosEdition::UosEducation == DSysInfo::uosEditionType()) {  //教育版登录界面不要显示系统版本号（和Logo冲突）
      //systemVersion = "";
      return;
    }

    m_logoVersionLabel->setText(getVersion());
    logoLayout->addWidget(m_logoVersionLabel);

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
        version = QString("%1 %2").arg(DSysInfo::majorVersion()).arg(DSysInfo::uosEditionName(m_locale));
    } else {
        version = QString("%1 %2").arg(DSysInfo::productVersion()).arg(DSysInfo::productTypeString());
    }
    return version;
}

void LogoWidget::updateLogoPixmap()
{
    QString logo = ":img/logo.svg";
    QString labelText = getVersion();
    bool loadFromDTK = true;
    // 家庭版更换水印logo
    if (DSysInfo::UosEducation == DSysInfo::uosEditionType()) {
        logo = QString(":img/distribution_logo.svg");
        labelText = "";
        loadFromDTK = false;
    }
    QPixmap pixmap = loadSystemLogo(logo, loadFromDTK);
    m_logoLabel->setPixmap(pixmap);
    m_logoVersionLabel->setText(labelText);
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
