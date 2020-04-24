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

#include "timewidget.h"
#include "../global_util/datetimehelper.h"

#include <QVBoxLayout>
#include <QDateTime>
#include <QFontDatabase>
#include <QSettings>

TimeWidget::TimeWidget(QWidget *parent)
    : QWidget(parent)
{
    int id = QFontDatabase::addApplicationFont("://fonts/NotoSansCJKsc-Thin.otf");
    const QString fontFamily = QFontDatabase::applicationFontFamilies(id).first();
    QFont timeFont(fontFamily);

    m_timeLabel = new QLabel;
    timeFont.setWeight(QFont::ExtraLight);
    m_timeLabel->setFont(timeFont);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    QPalette palette = m_timeLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_timeLabel->setPalette(palette);
    DFontSizeManager::instance()->bind(m_timeLabel, DFontSizeManager::T1);

    m_dateLabel = new QLabel;
    m_dateLabel->setAlignment(Qt::AlignCenter);
    palette = m_dateLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_dateLabel->setPalette(palette);
    DFontSizeManager::instance()->bind(m_dateLabel, DFontSizeManager::T6);

    refreshTime();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();

    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->addWidget(m_timeLabel);
    vLayout->addWidget(m_dateLabel);
    vLayout->setSpacing(0);
    vLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(vLayout);

    connect(m_refreshTimer, &QTimer::timeout, this, &TimeWidget::refreshTime);
}

void TimeWidget::setDateTimeFormat(bool use24HourFormat, int weekDayFormat, int shortDateFormat, int shortTimeFormat)
{
    m_use24HourFormat = use24HourFormat;
    m_weekDayFormat = weekDayFormat;
    m_shortDateFormat = shortDateFormat;
    m_shortTimeFormat = shortTimeFormat;
    refreshTime();
}

void TimeWidget::updateLocale(const QLocale &locale)
{
    m_locale = locale;
    refreshTime();
}

void TimeWidget::refreshTime()
{
//    if (m_use24HourFormat) {
//        m_timeLabel->setText(m_locale.toString(QDateTime::currentDateTime(), "hh:mm"));
//    } else {
//        m_timeLabel->setText(m_locale.toString(QDateTime::currentDateTime(), "hh:mm ap"));
//    }

//    m_dateLabel->setText(m_locale.toString(QDateTime::currentDateTime(), "yyyy-MM-dd dddd"));
    QDateTime dateTime = QDateTime::currentDateTime();

    QString timeFormat = DateTimeHelper::ShortTimeFormatString(m_shortTimeFormat);
    if (!m_use24HourFormat)
        timeFormat += " ap";
    QString dateFormat = DateTimeHelper::ShortDateFormatString(m_shortDateFormat) + " " + DateTimeHelper::WeekDayFormatString(m_weekDayFormat);

    m_timeLabel->setText(dateTime.toString(timeFormat));
    m_dateLabel->setText(dateTime.toString(dateFormat));
}
