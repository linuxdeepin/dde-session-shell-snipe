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

#include "sessionwidget.h"

#include "src/global_util/constants.h"
#include "src/session-widgets/sessionbasemodel.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>
#include <QLabel>
#include <QFile>
#include <QButtonGroup>
#include <QSettings>
#include <QPropertyAnimation>
#include <QString>

static const int SessionButtonWidth = 160;
static const int SessionButtonHeight = 160;

const QString session_standard_icon_name(const QString &realName)
{
    const QStringList standard_icon_list = {
        "deepin",
        "fluxbox",
        "gnome",
        "plasma",
        "ubuntu",
        "xfce"
    };

    for (const auto &name : standard_icon_list)
        if (realName.contains(name, Qt::CaseInsensitive))
            return name;

    return QStringLiteral("unknown");
}

SessionWidget *SessionWidget::getInstance(QWidget *parent)
{
    static SessionWidget w(parent);
    return &w;
}

SessionWidget::SessionWidget(QWidget *parent)
    : QFrame(parent)
    , m_currentSessionIndex(0)
    , m_frameDataBind(FrameDataBind::Instance())
    , m_sessionModel(new QLightDM::SessionsModel(this))
{
//    setStyleSheet("QFrame {"
//                  "background-color: red;"
//                  "}");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    loadSessionList();
    setFocusPolicy(Qt::StrongFocus);

    std::function<void (QVariant)> function = std::bind(&SessionWidget::onOtherPageChanged, this, std::placeholders::_1);
    int index = m_frameDataBind->registerFunction("SessionWidget", function);

    connect(this, &SessionWidget::destroyed, this, [ = ] {
        m_frameDataBind->unRegisterFunction("SessionWidget", index);
    });

    QTimer::singleShot(0, this, [ = ] {
        m_frameDataBind->refreshData("SessionWidget");
    });
}

void SessionWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

SessionWidget::~SessionWidget()
{
    qDeleteAll(m_sessionBtns);
}

const QString SessionWidget::currentSessionName() const
{
    return m_sessionModel->data(m_sessionModel->index(m_currentSessionIndex), QLightDM::SessionsModel::KeyRole).toString();
}

const QString SessionWidget::currentSessionKey() const
{
    return m_sessionModel->data(m_sessionModel->index(m_currentSessionIndex), QLightDM::SessionsModel::KeyRole).toString();
}

void SessionWidget::show()
{
    const int itemPadding = 20;
    const int itemWidth = m_sessionBtns.first()->width();
    const int itemTotal = itemPadding + itemWidth;

    // checked default session button
    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);

    const int cout = m_sessionBtns.size();
    const int maxLineCap = width() / itemWidth - 1; // 1 for left-offset and right-offset.
    const int offset = (width() - itemWidth * std::min(cout, maxLineCap)) / 2;

    int row = 0;
    int index = 0;
    for (auto it = m_sessionBtns.constBegin(); it != m_sessionBtns.constEnd(); ++it) {
        RoundItemButton *button = *it;
        // If the current value is the maximum, need to change the line.
        if (index >= maxLineCap) {
            index = 0;
            ++row;
        }

        QPropertyAnimation *ani = new QPropertyAnimation(button, "pos");
        ani->setStartValue(QPoint(width(), 0));
        ani->setEndValue(QPoint(QPoint(offset + index * itemWidth, itemTotal * row)));
        button->show();
        ani->start(QAbstractAnimation::DeleteWhenStopped);

        index++;
    }

    setFixedHeight(itemWidth * (row + 1));

    QWidget::show();
}

int SessionWidget::sessionCount() const
{
    return m_sessionModel->rowCount(QModelIndex());
}

const QString SessionWidget::lastSessionName() const
{
    QSettings setting(DDESESSIONCC::CONFIG_FILE + m_currentUser, QSettings::IniFormat);
    setting.beginGroup("User");
    const QString &session = setting.value("XSession").toString();

    return session.isEmpty() ? m_sessionModel->data(m_sessionModel->index(0), QLightDM::SessionsModel::KeyRole).toString() : session;
}

void SessionWidget::switchToUser(const QString &userName)
{
    qDebug() << "switch to user" << userName;
    if (m_currentUser != userName)
        m_currentUser = userName;

    const QString sessionName = lastSessionName();
    m_currentSessionIndex = sessionIndex(sessionName);

    m_model->setSessionKey(currentSessionKey());
    m_frameDataBind->updateValue("SessionWidget", m_currentSessionIndex);

    qDebug() << userName << "default session is: " << sessionName << m_currentSessionIndex;
}

void SessionWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        chooseSession();
        break;
    case Qt::Key_Left:
        leftKeySwitch();
        break;
    case Qt::Key_Right:
        rightKeySwitch();
        break;
    case Qt::Key_Escape:
        emit hideFrame();
        break;
    default:
        break;
    }
}

void SessionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    emit hideFrame();

    return QFrame::mouseReleaseEvent(event);
}

void SessionWidget::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, &SessionWidget::show);

    return QFrame::resizeEvent(event);
}

void SessionWidget::focusInEvent(QFocusEvent *)
{
    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
}

void SessionWidget::focusOutEvent(QFocusEvent *)
{
    m_sessionBtns.at(m_currentSessionIndex)->setChecked(false);
}

void SessionWidget::onSessionButtonClicked()
{
    RoundItemButton *btn = qobject_cast<RoundItemButton *>(sender());
    Q_ASSERT(btn);
    Q_ASSERT(m_sessionBtns.contains(btn));

    btn->setChecked(true);
    m_currentSessionIndex = m_sessionBtns.indexOf(btn);

    m_model->setSessionKey(currentSessionKey());

    emit hideFrame();

//    emit sessionChanged(currentSessionName());
}

int SessionWidget::sessionIndex(const QString &sessionName)
{
    const int count = m_sessionModel->rowCount(QModelIndex());
    Q_ASSERT(count);
    for (int i(0); i != count; ++i)
        if (!sessionName.compare(m_sessionModel->data(m_sessionModel->index(i), QLightDM::SessionsModel::KeyRole).toString(), Qt::CaseInsensitive))
            return i;

    // NOTE: The current session does not exist
    qWarning() << "The session does not exist, using the default value.";
    return 0;
}

void SessionWidget::onOtherPageChanged(const QVariant &value)
{
    const int index = value.toInt();

    qDebug() << index;

    if (index == m_currentSessionIndex) return;

    for (RoundItemButton *button : m_sessionBtns) {
        button->setChecked(false);
    }

    m_sessionBtns.at(index)->setChecked(true);
    m_currentSessionIndex = index;
}

void SessionWidget::leftKeySwitch()
{
    if (m_currentSessionIndex == 0) {
        m_currentSessionIndex = m_sessionBtns.size() - 1;
    } else {
        m_currentSessionIndex -= 1;
    }

    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
    m_frameDataBind->updateValue("SessionWidget", m_currentSessionIndex);
}

void SessionWidget::rightKeySwitch()
{
    if (m_currentSessionIndex == m_sessionBtns.size() - 1) {
        m_currentSessionIndex = 0;
    } else {
        m_currentSessionIndex += 1;
    }

    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
    m_frameDataBind->updateValue("SessionWidget", m_currentSessionIndex);
}

void SessionWidget::chooseSession()
{
    emit m_sessionBtns.at(m_currentSessionIndex)->clicked();
    emit hideFrame();
}

void SessionWidget::loadSessionList()
{
    // add sessions button
    const int count = m_sessionModel->rowCount(QModelIndex());
    for (int i(0); i != count; ++i) {
        const QString &session_name = m_sessionModel->data(m_sessionModel->index(i), QLightDM::SessionsModel::KeyRole).toString();
        const QString &session_icon = session_standard_icon_name(session_name);
        const QString normalIcon = QString(":/img/sessions_icon/%1_normal.svg").arg(session_icon);
        const QString hoverIcon = QString(":/img/sessions_icon/%1_hover.svg").arg(session_icon);
        const QString checkedIcon = QString(":/img/sessions_icon/%1_press.svg").arg(session_icon);

        qDebug() << "found session: " << session_name << session_icon;
        RoundItemButton *sbtn = new RoundItemButton(session_name, this);
        sbtn->setFixedSize(SessionButtonWidth, SessionButtonHeight);
        sbtn->setAutoExclusive(true);
        sbtn->setProperty("normalIcon", normalIcon);
        sbtn->setProperty("hoverIcon", hoverIcon);
        sbtn->setProperty("checkedIcon", checkedIcon);
        sbtn->hide();
        sbtn->setFocusPolicy(Qt::NoFocus);

        connect(sbtn, &RoundItemButton::clicked, this, &SessionWidget::onSessionButtonClicked);

        m_sessionBtns.append(sbtn);
    }
}
