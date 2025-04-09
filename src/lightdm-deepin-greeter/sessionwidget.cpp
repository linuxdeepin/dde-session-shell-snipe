// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionwidget.h"
#include "sessionbasemodel.h"
#include "dconfig_helper.h"

#include <QApplication>
#include <QLabel>
#include <QFile>
#include <QSettings>
#include <QPropertyAnimation>
#include <QString>

#include "dbusconstant.h"

static const int SessionButtonWidth = 160;
static const int SessionButtonHeight = 160;
static const QString DEFAULT_SESSION_NAME = "deepin";
static const QString WAYLAND_SESSION_NAME = "Wayland";

const QString session_standard_icon_name(const QString &realName)
{
    const QStringList standard_icon_list = {
        "deepin",
        "fluxbox",
        "gnome",
        "plasma",
        "ubuntu",
        "xfce",
        "wayland"
    };

    for (const auto &name : standard_icon_list) {
        if (realName.contains(name, Qt::CaseInsensitive)) {
            if (realName == "deepin") {
                return "x11";
            } else {
                return name;
            }
        }
    }
    return QStringLiteral("unknown");
}

SessionWidget::SessionWidget(QWidget *parent)
    : QFrame(parent)
    , m_currentSessionIndex(0)
    , m_sessionModel(new QLightDM::SessionsModel(this))
    , m_userModel(new QLightDM::UsersModel(this))
    , m_allowSwitchingToWayland(DConfigHelper::instance()->getConfig("allowSwitchingToWayland", false).toBool())
    , m_isWaylandExisted(false)
    , m_warningLabel(new QLabel(this))
    , m_defaultSession(DConfigHelper::instance()->getConfig("defaultSession", DEFAULT_SESSION_NAME).toString())
    , m_resetSessionIndex(true)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    loadSessionList();
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("SessionWidget");
    m_warningLabel->setText(tr("You have enabled the high system security level, thus cannot switch to the Wayland mode, "\
                               "please disable the high security level in Security Center and try again."));
    m_warningLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_warningLabel->setAlignment(Qt::AlignCenter);
    m_warningLabel->hide();

    // 判断显卡是否支持wayland
    if (m_allowSwitchingToWayland) {
        QDBusInterface systemDisplayInter(DSS_DBUS::systemDisplayService, DSS_DBUS::systemDisplayPath,
                DSS_DBUS::systemDisplayService, QDBusConnection::systemBus(), this);
        QDBusReply<bool> reply  = systemDisplayInter.call("SupportWayland");
        if (QDBusError::NoError == reply.error().type())
            m_allowSwitchingToWayland = reply.value();
        else
            qCWarning(DDE_SHELL) << "Get support wayland property failed: " << reply.error().message();
    }
}

void SessionWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

SessionWidget::~SessionWidget()
{
    qDeleteAll(m_sessionBtns);
}

const QString SessionWidget::currentSessionKey() const
{
    return m_sessionModel->data(m_sessionModel->index(m_currentSessionIndex), QLightDM::SessionsModel::KeyRole).toString();
}

void SessionWidget::updateLayout()
{
    const int itemPadding = 20;
    const int itemWidth = m_sessionBtns.first()->width();
    const int itemHeight = m_sessionBtns.first()->height();
    const int itemTotal = itemPadding + itemWidth;

    // checked default session button
    if (m_resetSessionIndex)
        m_currentSessionIndex = sessionIndex(m_model->sessionKey());
    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
    m_resetSessionIndex = false;

    int count = m_sessionBtns.size();
    if (m_isWaylandExisted && !m_allowSwitchingToWayland)
        --count;

    const int maxLineCap = width() / itemWidth - 1; // 1 for left-offset and right-offset.
    const int offset = (width() - itemWidth * std::min(count, maxLineCap)) / 2;


    int row = 0;
    int index = 0;
    for (auto it = m_sessionBtns.constBegin(); it != m_sessionBtns.constEnd(); ++it) {
        RoundItemButton *button = *it;
        if (!m_allowSwitchingToWayland && !WAYLAND_SESSION_NAME.compare(button->text(), Qt::CaseInsensitive))
            continue;
        // If the current value is the maximum, need to change the line.
        if (index >= maxLineCap) {
            index = 0;
            ++row;
        }

        QPropertyAnimation *ani = new QPropertyAnimation(button, "pos");
        ani->setStartValue(QPoint(width(), 50));
        ani->setEndValue(QPoint(QPoint(offset + index * itemWidth, itemTotal * row + 50)));
        button->show();
        ani->start(QAbstractAnimation::DeleteWhenStopped);

        index++;
    }

    m_warningLabel->setContentsMargins(0, 10, 0, 10);
    m_warningLabel->setFixedSize(width(), 50);
    m_warningLabel->hide();

    setFixedHeight(itemHeight * (row + 1) + 50);
}

int SessionWidget::sessionCount() const
{
    int count = m_sessionModel->rowCount(QModelIndex());
    if (m_isWaylandExisted && !m_allowSwitchingToWayland)
        --count;

    return count;
}

void SessionWidget::switchToUser(const QString &userName)
{
    qDebug() << "Switch to user" << userName;
    if (m_currentUser != userName)
        m_currentUser = userName;

    QString sessionName = lastLoggedInSession(userName);
    // 上次登录的是wayland，但是此次登录已经禁止使用wayland，那么使用默认的桌面
    if ((!m_allowSwitchingToWayland && !WAYLAND_SESSION_NAME.compare(sessionName, Qt::CaseInsensitive)) ||
         !WAYLAND_SESSION_NAME.compare(m_defaultSession, Qt::CaseInsensitive))
        sessionName = m_defaultSession;
    m_currentSessionIndex = sessionIndex(sessionName);

    m_model->setSessionKey(currentSessionKey());

    qDebug() << "User name: " << userName << ", default session: " << sessionName << ", current session index: " << m_currentSessionIndex;
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
        hideSessionFrame();
        break;
    default:
        break;
    }
}

void SessionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    hideSessionFrame();

    return QFrame::mouseReleaseEvent(event);
}

void SessionWidget::resizeEvent(QResizeEvent *event)
{
    if (isVisible())
        QTimer::singleShot(0, this, &SessionWidget::updateLayout);

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

    QString sessionKey = m_sessionModel->data(m_sessionModel->index(m_sessionBtns.indexOf(btn)), QLightDM::SessionsModel::KeyRole).toString();

    if (!WAYLAND_SESSION_NAME.compare(sessionKey, Qt::CaseInsensitive) && m_model->getSEType()) {
        // 在开启等保（高）的情况下不允许切换到wayland环境
        m_warningLabel->show();
        return;
    }

    btn->setChecked(true);
    m_currentSessionIndex = m_sessionBtns.indexOf(btn);

    m_model->setSessionKey(currentSessionKey());

    hideSessionFrame();
    m_warningLabel->hide();
}

int SessionWidget::sessionIndex(const QString &sessionName)
{
    const int count = m_sessionModel->rowCount(QModelIndex());
    Q_ASSERT(count);
    int defaultSessionIndex = 0;
    for (int i(0); i < count; ++i) {
        const QString &sessionKey = m_sessionModel->data(m_sessionModel->index(i), QLightDM::SessionsModel::KeyRole).toString();
        if (!sessionName.compare(sessionKey, Qt::CaseInsensitive))
            return i;

        if (!DEFAULT_SESSION_NAME.compare(sessionKey, Qt::CaseInsensitive))
            defaultSessionIndex = i;
    }

    // NOTE: The current session does not exist
    qCWarning(DDE_SHELL) << "The session does not exist, using the default value.";
    return defaultSessionIndex;
}

void SessionWidget::leftKeySwitch()
{
    if (m_currentSessionIndex == 0) {
        m_currentSessionIndex = m_sessionBtns.size() - 1;
    } else {
        m_currentSessionIndex -= 1;
    }

    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
}

void SessionWidget::rightKeySwitch()
{
    if (m_currentSessionIndex == m_sessionBtns.size() - 1) {
        m_currentSessionIndex = 0;
    } else {
        m_currentSessionIndex += 1;
    }

    m_sessionBtns.at(m_currentSessionIndex)->setChecked(true);
}

void SessionWidget::chooseSession()
{
    emit m_sessionBtns.at(m_currentSessionIndex)->clicked();
    hideSessionFrame();
}

void SessionWidget::loadSessionList()
{
    // add sessions button
    const int count = m_sessionModel->rowCount(QModelIndex());
    for (int i(0); i < count; ++i) {
        const QString &session_name = m_sessionModel->data(m_sessionModel->index(i), QLightDM::SessionsModel::KeyRole).toString();
        const QString &session_icon = session_standard_icon_name(session_name);
        const QString normalIcon = QString(":/img/sessions_icon/%1_normal.svg").arg(session_icon);
        const QString hoverIcon = QString(":/img/sessions_icon/%1_hover.svg").arg(session_icon);
        const QString pressIcon = QString(":/img/sessions_icon/%1_press.svg").arg(session_icon);

        qDebug() << "Load session list, found session, session name: " << session_name << "session icon: " << session_icon;
        RoundItemButton *sbtn = nullptr;
        if (session_name == "deepin") {
            sbtn = new RoundItemButton("X11", this);
        } else {
            sbtn = new RoundItemButton(session_name, this);
        }
        sbtn->setAccessibleName("RoundItemButton");
        sbtn->setAutoExclusive(true);
        sbtn->setProperty("normalIcon", normalIcon);
        sbtn->setProperty("hoverIcon", hoverIcon);
        sbtn->setProperty("pressedIcon", pressIcon);
        sbtn->hide();
        sbtn->setFocusPolicy(Qt::NoFocus);

        connect(sbtn, &RoundItemButton::clicked, this, &SessionWidget::onSessionButtonClicked);

        m_sessionBtns.append(sbtn);

        if (!WAYLAND_SESSION_NAME.compare(session_name, Qt::CaseInsensitive))
            m_isWaylandExisted = true;
    }
}

QString SessionWidget::lastLoggedInSession(const QString &userName)
{
    for (int i = 0; i < m_userModel->rowCount(QModelIndex()); ++i) {
        if (userName == m_userModel->data(m_userModel->index(i), QLightDM::UsersModel::NameRole).toString()) {
            QString session = m_userModel->data(m_userModel->index(i), QLightDM::UsersModel::SessionRole).toString();
            if (!session.isEmpty())
                return session;
        }
    }

    return m_defaultSession;
}

void SessionWidget::showEvent(QShowEvent *event)
{
    updateLayout();
    QWidget::showEvent(event);
}

void SessionWidget::hideSessionFrame()
{
    m_resetSessionIndex = true;
    emit notifyHideSessionFrame();
}
