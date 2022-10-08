// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "centertopwidget.h"
#include "public_func.h"

#include <QVBoxLayout>
#include <QResizeEvent>

DCORE_USE_NAMESPACE

const QString SHOW_TOP_TIP = "showTopTip";
const QString TOP_TIP_TEXT = "topTipText";
const int TOP_TIP_MAX_LENGTH = 60;
const int TOP_TIP_SPACING = 10;

CenterTopWidget::CenterTopWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentUser(nullptr)
    , m_timeWidget(new TimeWidget(this))
    , m_topTip(new QLabel(this))
    , m_topTipSpacer(new QSpacerItem(0, TOP_TIP_SPACING))
    , m_config(DConfig::create(getDefaultConfigFileName(), getDefaultConfigFileName(), QString(), this))
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
    DFontSizeManager::instance()->bind(m_topTip, DFontSizeManager::T6);
    layout->addSpacerItem(m_topTipSpacer);
    layout->addWidget(m_topTip);
    bool showTopTip = false;
    QString topTipText = "";
    if (m_config) {
        if (m_config->keyList().contains(TOP_TIP_TEXT))
            topTipText = m_config->value(TOP_TIP_TEXT).toString();

        if (m_config->keyList().contains(SHOW_TOP_TIP))
            showTopTip = m_config->value(SHOW_TOP_TIP).toBool();

        // 即时响应数据变化
        connect(m_config, &DConfig::valueChanged, this, [this] (const QString &key) {
            if (key == SHOW_TOP_TIP) {
                const bool showTopTip = m_config->value(SHOW_TOP_TIP).toBool();
                m_topTipSpacer->changeSize(0, showTopTip ? TOP_TIP_SPACING : 0);
                m_topTip->setVisible(showTopTip);
            } else if (key == TOP_TIP_TEXT) {
                setTopTipText(m_config->value(TOP_TIP_TEXT).toString());
            }
        });
    }

    m_topTipSpacer->changeSize(0, showTopTip ? TOP_TIP_SPACING : 0);
    m_topTip->setVisible(showTopTip);
    setTopTipText(topTipText);
}

void CenterTopWidget::setCurrentUser(User *user)
{
    if (!user || m_currentUser.data() == user) {
        return;
    }

    m_currentUser = QPointer<User>(user);
    if (!m_currentUser.isNull()) {
        for (auto connect : m_currentUserConnects) {
            m_currentUser->disconnect(connect);
        }
    }

    auto locale = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : user->locale();
    m_timeWidget->updateLocale(locale);
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
}


void CenterTopWidget::updateTimeFormat(bool use24)
{
    if (!m_currentUser.isNull()) {
        auto locale = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : m_currentUser->locale();
        m_timeWidget->updateLocale(locale);
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
