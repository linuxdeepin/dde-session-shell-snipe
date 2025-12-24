// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "centertopwidget.h"
#include "public_func.h"
#include "dconfig_helper.h"
#include "constants.h"

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QMetaEnum>

DCORE_USE_NAMESPACE
using namespace DDESESSIONCC;

const int TOP_TIP_MAX_LENGTH = 60;
const int TOP_TIP_SPACING = 10;

#ifdef ENABLE_DSS_SNIPE
static const QString localeNameKey = "localeName";
static const QString shortDateFormatKey = "shortDateFormat";
static const QString shortTimeFormatKey = "shortTimeFormat";
#endif // ENABLE_DSS_SNIPE

CenterTopWidget::CenterTopWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentUser(nullptr)
    , m_timeWidget(new TimeWidget(this))
    , m_topTip(new QLabel(this))
    , m_topTipSpacer(new QSpacerItem(0, TOP_TIP_SPACING))
{
    initUi();
}

void CenterTopWidget::initUi()
{
    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);
    m_timeWidget->setAccessibleName("TimeWidget");
    // 处理时间制跳转策略，获取到时间制再显示时间窗口
    m_timeWidget->setVisible(false);
    layout->addWidget(m_timeWidget, 1);
    // 顶部提示语，一般用于定制项目，主线默认不展示
    m_topTip->setAlignment(Qt::AlignCenter);
    QPalette palette = m_topTip->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_topTip->setPalette(palette);
    // 设置字体大小
    bool ok;
    int fontSize = DConfigHelper::instance()->getConfig(TOP_TIP_TEXT_FONT, 5).toInt(&ok);
    if (!ok || fontSize < 0 || fontSize > 9) fontSize = DFontSizeManager::T6;
    DFontSizeManager::instance()->bind(m_topTip, static_cast<DFontSizeManager::SizeType>(fontSize));

    layout->addSpacerItem(m_topTipSpacer);
    layout->addWidget(m_topTip);
    const bool showTopTip = DConfigHelper::instance()->getConfig(SHOW_TOP_TIP, false).toBool();
    m_topTipSpacer->changeSize(0, showTopTip ? TOP_TIP_SPACING : 0);
    m_topTip->setVisible(showTopTip);
    setTopTipText(DConfigHelper::instance()->getConfig(TOP_TIP_TEXT, "").toString());

    DConfigHelper::instance()->bind(this, SHOW_TOP_TIP, &CenterTopWidget::onDConfigPropertyChanged);
    DConfigHelper::instance()->bind(this, TOP_TIP_TEXT, &CenterTopWidget::onDConfigPropertyChanged);
    DConfigHelper::instance()->bind(this, TOP_TIP_TEXT_FONT, &CenterTopWidget::onDConfigPropertyChanged);
}

void CenterTopWidget::setCurrentUser(User *user)
{
    if (!user || m_currentUser.data() == user) {
        return;
    }

#ifdef ENABLE_DSS_SNIPE
    updateRegionFormatConnection(user);
#endif // ENABLE_DSS_SNIPE
    m_currentUser = QPointer<User>(user);
    if (!m_currentUser.isNull()) {
        for (auto connect : m_currentUserConnects) {
            m_currentUser->disconnect(connect);
        }
    }

#ifdef ENABLE_DSS_SNIPE
    m_timeWidget->updateLocale(QLocale(getUserLocale(user)));
#else
    auto locale = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : user->locale();
    m_timeWidget->updateLocale(QLocale(locale));
#endif // ENABLE_DSS_SNIPE
    m_timeWidget->set24HourFormat(user->isUse24HourFormat());
    m_timeWidget->setWeekdayFormatType(user->weekdayFormat());
    m_timeWidget->setShortDateFormat(user->shortDateFormat());
    m_timeWidget->setShortTimeFormat(user->shortTimeFormat());

    m_currentUserConnects << connect(user, &User::use24HourFormatChanged, this, &CenterTopWidget::updateTimeFormat, Qt::UniqueConnection)
                          << connect(user, &User::weekdayFormatChanged, m_timeWidget, &TimeWidget::setWeekdayFormatType)
                          << connect(user, &User::shortDateFormatChanged, m_timeWidget, &TimeWidget::setShortDateFormat)
                          << connect(user, &User::shortTimeFormatChanged, m_timeWidget, &TimeWidget::setShortTimeFormat);

    QTimer::singleShot(0, this, [this, user] {
        updateTimeFormat(user->isUse24HourFormat());
    });

#ifdef ENABLE_DSS_SNIPE
    updateUserDateTimeFormat();
#endif // ENABLE_DSS_SNIPE
}


void CenterTopWidget::updateTimeFormat(bool use24)
{
    if (!m_currentUser.isNull()) {
#ifdef ENABLE_DSS_SNIPE
        m_timeWidget->updateLocale(QLocale(getUserLocale(m_currentUser)));
#else
        auto locale = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : m_currentUser->locale();
        m_timeWidget->updateLocale(QLocale(locale));
#endif // ENABLE_DSS_SNIPE
        m_timeWidget->set24HourFormat(use24);
        m_timeWidget->setVisible(true);
    }
}

void CenterTopWidget::setTopTipText(const QString &text)
{
    QString firstLine = text.split("\n").first();
    if (firstLine.length() > TOP_TIP_MAX_LENGTH) {
        // 居中省略号，理论上需要判断字体是否支持，考虑到这种场景非常少，
        // 而且省略号处理只是锦上添花，可以忽略这个判断。
        QChar ellipsisChar(0x2026);
        firstLine = firstLine.left(60) + QString(ellipsisChar);
    }
    m_tipText = firstLine;
    updateTopTipWidget();
}

QSize CenterTopWidget::sizeHint() const
{
    const int heightHint = m_timeWidget->sizeHint().height() + m_topTip->fontMetrics().height() + TOP_TIP_SPACING;

    return QSize(QWidget::sizeHint().width(), heightHint);
}

void CenterTopWidget::resizeEvent(QResizeEvent *event)
{
    updateTopTipWidget();
    QWidget::resizeEvent(event);
}

void CenterTopWidget::updateTopTipWidget()
{
    QString firstLine = m_tipText;
    const int maxWidth = topLevelWidget()->geometry().width() - 30;
    if (m_topTip->fontMetrics().boundingRect(firstLine).width() > maxWidth)
        firstLine = m_topTip->fontMetrics().elidedText(firstLine, Qt::ElideRight, maxWidth);

    m_topTip->setText(firstLine);
}

void CenterTopWidget::onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<CenterTopWidget*>(objPtr);
    if (!obj)
        return;

    if (key == SHOW_TOP_TIP) {
        const bool showTopTip = value.toBool();
        obj->m_topTipSpacer->changeSize(0, showTopTip ? TOP_TIP_SPACING : 0);
        obj->m_topTip->setVisible(showTopTip);
    } else if (key == TOP_TIP_TEXT) {
        obj->setTopTipText(value.toString());
    } else if (key == TOP_TIP_TEXT_FONT) {
        bool ok;
        const int fontSize = value.toInt(&ok);
        if (ok && fontSize > 0 && fontSize < 9)
            DFontSizeManager::instance()->bind(obj->m_topTip, static_cast<DFontSizeManager::SizeType>(fontSize));
        else
            qCWarning(DDE_SHELL) << "Top tip text font format error, font size: " << fontSize;
    }
}

#ifdef ENABLE_DSS_SNIPE
void CenterTopWidget::onUserRegionFormatValueChanged(const QDBusMessage &dbusMessage)
{
    QList<QVariant> arguments = dbusMessage.arguments();
    if (1 != arguments.count())
        return;

    QString key = dbusMessage.arguments().at(0).toString();
    if (key == localeNameKey || key == shortTimeFormatKey || key == shortDateFormatKey) {
        updateUserDateTimeFormat();
    }
}

QString CenterTopWidget::getRegionFormatConfigPath(const User *user) const
{
    if (!user)
        return QString();

    QDBusInterface configInter("org.desktopspec.ConfigManager", "/", "org.desktopspec.ConfigManager", QDBusConnection::systemBus());
    if (!configInter.isValid()) {
        qWarning("Can't acquire config manager. error:\"%s\"",
                 qPrintable(QDBusConnection::systemBus().lastError().message()));
        return QString();
    }
    QDBusReply<QDBusObjectPath> dbusReply = configInter.call("acquireManagerV2",
                                                             (uint)user->uid(),
                                                             QString(QCoreApplication::applicationName()),
                                                             QString("org.deepin.region-format"),
                                                             QString(""));
    if (configInter.lastError().isValid() ) {
        qWarning("Call failed: %s", qPrintable(configInter.lastError().message()));
        return QString();
    }
    return dbusReply.value().path();
}

QString CenterTopWidget::getRegionFormatValue(const QString &userConfigDbusPath, const QString &key) const
{
    if (userConfigDbusPath.isEmpty())
        return QString();
    QDBusInterface managerInter("org.desktopspec.ConfigManager", userConfigDbusPath, "org.desktopspec.ConfigManager.Manager", QDBusConnection::systemBus());
    QDBusReply<QVariant> reply = managerInter.call("value", key);
    if (managerInter.lastError().isValid() ) {
        qWarning("Call failed: %s", qPrintable(managerInter.lastError().message()));
        return QString();
    }
    return reply.value().toString();
}

QString CenterTopWidget::getUserLocale(const User *user) const
{
    if (!user)
        return QLocale::system().name();

    if (qApp->applicationName() == "dde-lock")
        return QLocale::system().name();

    QString userConfigDbusPath = getRegionFormatConfigPath(user);
    QString localeName = getRegionFormatValue(userConfigDbusPath, localeNameKey);
    if (localeName.isEmpty()) {
        localeName = user->locale();
    }
    return localeName;
}

void CenterTopWidget::updateRegionFormatConnection(const User *user)
{
    if (!user) {
        return;
    }

    // disconnect old user
    if (m_currentUser) {
        QString dbusPath = getRegionFormatConfigPath(m_currentUser);
        if (dbusPath.isEmpty())
            return;
        QDBusConnection::systemBus().disconnect("org.desktopspec.ConfigManager", dbusPath, "org.desktopspec.ConfigManager.Manager",
                                                "valueChanged", "s", this,
                                                SLOT(onUserRegionFormatValueChanged(const QDBusMessage&))); 
    }

    // connect new user
    QString dbusPath = getRegionFormatConfigPath(user);
    if (dbusPath.isEmpty())
        return;
    
    QDBusConnection::systemBus().connect("org.desktopspec.ConfigManager", dbusPath, "org.desktopspec.ConfigManager.Manager",
                                         "valueChanged", "s", this,
                                         SLOT(onUserRegionFormatValueChanged(const QDBusMessage&)));
}

void CenterTopWidget::updateUserDateTimeFormat()
{
    QString userConfigDbusPath = getRegionFormatConfigPath(m_currentUser);

    QString localeName = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : getRegionFormatValue(userConfigDbusPath, localeNameKey);
    QString shortTimeFormat = getRegionFormatValue(userConfigDbusPath, shortTimeFormatKey);
    QString shortDateFormat = getRegionFormatValue(userConfigDbusPath, shortDateFormatKey);

    m_timeWidget->updateLocale(localeName, shortTimeFormat, shortDateFormat);
}
#endif // ENABLE_DSS_SNIPE
