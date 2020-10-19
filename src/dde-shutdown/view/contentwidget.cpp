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

#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtCore/QObject>
#include <DDBusSender>

#include <libintl.h>

#include "src/global_util/dbus/dbusvariant.h"
#include "contentwidget.h"
#include "multiuserswarningview.h"
#include "inhibitwarnview.h"

#include "src/global_util/dbus/dbuscontrolcenter.h"

#include "src/session-widgets/sessionbasemodel.h"
#include "src/global_util/public_func.h"
#include "src/global_util/constants.h"

ContentWidget::ContentWidget(QWidget *parent)
    : QFrame(parent)
    , m_login1Inter(new DBusLogin1Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this))
    , m_switchosInterface(new HuaWeiSwitchOSInterface("com.huawei", "/com/huawei/switchos", QDBusConnection::sessionBus(), this))
    , m_controlCenterInter(new DBusControlCenter(this))
    , m_systemMonitor(nullptr)
    , m_wmInter(new com::deepin::wm("com.deepin.wm", "/com/deepin/wm", QDBusConnection::sessionBus(), this))
    , m_dbusAppearance(new Appearance("com.deepin.daemon.Appearance",
                                      "/com/deepin/daemon/Appearance",
                                      QDBusConnection::sessionBus(),
                                      this))

{
    initUI();
    initData();
    initConnect();
}

void ContentWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;

    connect(model, &SessionBaseModel::onUserListChanged, this, &ContentWidget::onUserListChanged);
    onUserListChanged(model->userList());

    connect(model, &SessionBaseModel::onHasSwapChanged, this, &ContentWidget::enableHibernateBtn);
    enableHibernateBtn(model->hasSwap());

    connect(model, &SessionBaseModel::canSleepChanged, this, &ContentWidget::enableSleepBtn);
    enableSleepBtn(model->canSleep());
}

ContentWidget::~ContentWidget()
{

}

void ContentWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // NOTE: When the signal is sent after the press,
    // the other window receives the mouse release event,
    // causing the program to be started again

    onCancel();

    QFrame::mouseReleaseEvent(event);
}

void ContentWidget::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);

    DDBusSender()
    .service("com.deepin.dde.Launcher")
    .interface("com.deepin.dde.Launcher")
    .path("/com/deepin/dde/Launcher")
    .method("Hide")
    .call();

    // hide dde-control-center
    if (m_controlCenterInter->isValid())
        m_controlCenterInter->HideImmediately();

    QTimer::singleShot(1, this, [ = ] {
        activateWindow();
    });

    if (m_warningView) {
        m_mainLayout->setCurrentWidget(m_warningView);
    }

    tryGrabKeyboard();

    m_currentSelectedBtn = m_lockButton;
    m_currentSelectedBtn->updateState(RoundItemButton::Checked);
}

bool ContentWidget::handleKeyPress(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape: onCancel(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter: enterKeyPushed(); break;
    case Qt::Key_Tab: {
        if (m_systemMonitor && m_currentSelectedBtn->isVisible() && m_systemMonitor->isVisible()) {
            if (m_currentSelectedBtn && m_currentSelectedBtn->isChecked()) {
                m_currentSelectedBtn->setChecked(false);
                m_systemMonitor->setState(SystemMonitor::Enter);
                return true;
            }

            if (m_systemMonitor->state() == SystemMonitor::Enter) {
                m_systemMonitor->setState(SystemMonitor::Leave);
                m_currentSelectedBtn->setChecked(true);
                return true;
            }
        }
        break;
    }
    case Qt::Key_Left: {
        if (m_systemMonitor)
            m_systemMonitor->setState(SystemMonitor::Leave);
        setPreviousChildFocus();
        break;
    }
    case Qt::Key_Right: {
        if (m_systemMonitor)
            m_systemMonitor->setState(SystemMonitor::Leave);
        setNextChildFocus();
        break;
    }
    case Qt::Key_Up: {
        if (m_warningView) {
            m_warningView->toggleButtonState();
            break;
        }

        if (m_systemMonitor) {
            m_systemMonitor->setState(SystemMonitor::Leave);
        }
        m_currentSelectedBtn->setChecked(true);
        break;
    }
    case Qt::Key_Down: {
        if (m_warningView) {
            m_warningView->toggleButtonState();
            break;
        }

        if (m_systemMonitor) {
            m_currentSelectedBtn->updateState(RoundItemButton::Normal);
            m_systemMonitor->setState(SystemMonitor::Enter);
        }
        break;
    }
    default:;
    }

    return false;
}

void ContentWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    m_buttonSpacer->changeSize(0, (height() - m_shutdownButton->height()) / 2);
}

bool ContentWidget::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        return handleKeyPress(key_event);
    }

    return QWidget::event(event);
}

void ContentWidget::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
}

void ContentWidget::setConfirm(const bool confirm)
{
    m_confirm = confirm;
}

bool ContentWidget::powerAction(const Actions action)
{
    switch (action) {
    case Shutdown:
    case Restart:
    case Suspend:
    case Hibernate:
    case SwitchSystem:
    case Logout:
        return beforeInvokeAction(action);
    default:
        shutDownFrameActions(action);
    }

    return true;
}

void ContentWidget::initConnect()
{
    connect(m_shutdownButton, &RoundItemButton::clicked, [this] { emit buttonClicked(Shutdown);});
    connect(m_restartButton, &RoundItemButton::clicked, [this] { emit buttonClicked(Restart);});
    connect(m_suspendButton, &RoundItemButton::clicked, [this] { emit buttonClicked(Suspend);});
    connect(m_hibernateButton, &RoundItemButton::clicked, [ = ] {emit buttonClicked(Hibernate);});
    connect(m_lockButton, &RoundItemButton::clicked, [this] { shutDownFrameActions(Lock);});
    connect(m_logoutButton, &RoundItemButton::clicked, [this] {emit buttonClicked(Logout);});
    connect(m_switchUserBtn, &RoundItemButton::clicked, [this] { shutDownFrameActions(SwitchUser);});
    connect(m_wmInter, &__wm::WorkspaceSwitched, this, &ContentWidget::currentWorkspaceChanged);

    if (m_systemMonitor) {
        connect(m_systemMonitor, &SystemMonitor::clicked, this, &ContentWidget::runSystemMonitor);
    }

    if(m_switchSystemBtn) {
        connect(m_switchSystemBtn, &RoundItemButton::clicked, [this] { emit buttonClicked(SwitchSystem); });
    }
}

void ContentWidget::initData()
{
    m_sessionInterface = new DBusSessionManagerInterface("com.deepin.SessionManager", "/com/deepin/SessionManager",
                                                         QDBusConnection::sessionBus(), this);
}

void ContentWidget::enterKeyPushed()
{
    if (m_warningView) {
        m_warningView->buttonClickHandle();
        return;
    }

    if (m_systemMonitor && m_systemMonitor->state() == SystemMonitor::Enter) {
        runSystemMonitor();
        return;
    }

    if (m_warningView && m_warningView->isVisible()) return;

    if (m_currentSelectedBtn->isDisabled())
        return;

    if (m_currentSelectedBtn == m_shutdownButton)
        beforeInvokeAction(Shutdown);
    else if (m_currentSelectedBtn == m_restartButton)
        beforeInvokeAction(Restart);
    else if (m_currentSelectedBtn == m_suspendButton)
        beforeInvokeAction(Suspend);
    else if (m_currentSelectedBtn == m_lockButton)
        shutDownFrameActions(Lock);
    else if (m_currentSelectedBtn == m_logoutButton)
        beforeInvokeAction(Logout);
    else if (m_currentSelectedBtn == m_switchUserBtn)
        shutDownFrameActions(SwitchUser);
    else if (m_currentSelectedBtn == m_switchSystemBtn)
        beforeInvokeAction(SwitchSystem);
    else if (m_currentSelectedBtn == m_hibernateButton)
        beforeInvokeAction(Hibernate);
}

void ContentWidget::hideBtn(const QString &btnName)
{
    std::map<RoundItemButton *, QString> map{
        { m_shutdownButton, "Shutdown" },    { m_restartButton, "Restart" },
        { m_suspendButton, "Suspend" },      { m_lockButton, "Lock" },
        { m_logoutButton, "Logout" },        { m_switchUserBtn, "SwitchUser" },
        { m_hibernateButton, "Hibernate" },  { m_switchSystemBtn, "SwitchSystem" }
    };

    for (auto result : map) {
        if (!btnName.compare(result.second, Qt::CaseInsensitive)) {
            if(result.first) { result.first->hide(); return; }
        }
    }
}

void ContentWidget::disableBtn(const QString &btnName)
{
    std::map<RoundItemButton *, QString> map{
        { m_shutdownButton, "Shutdown" },    { m_restartButton, "Restart" },
        { m_suspendButton, "Suspend" },      { m_lockButton, "Lock" },
        { m_logoutButton, "Logout" },        { m_switchUserBtn, "SwitchUser" },
        { m_hibernateButton, "Hibernate" },  { m_switchSystemBtn, "SwitchSystem" }
    };

    for (auto result : map) {
        if (!btnName.compare(result.second, Qt::CaseInsensitive)) {
            if(result.first) { result.first->hide(); return; }
        }
    }
}

bool ContentWidget::beforeInvokeAction(const Actions action)
{
    const QList<InhibitWarnView::InhibitorData> inhibitors = listInhibitors(action);

    const QList<std::shared_ptr<User>> &loginUsers = m_model->logindUser();

    // change ui
    if (m_confirm || !inhibitors.isEmpty() || loginUsers.length() > 1) {
        for (RoundItemButton *btn : *m_btnsList) {
            btn->hide();
        }

        if (m_warningView != nullptr) {
            m_mainLayout->removeWidget(m_warningView);
            m_warningView->deleteLater();
            m_warningView = nullptr;
        }
    }

    if (!inhibitors.isEmpty()) {
        InhibitWarnView *view = new InhibitWarnView(action, this);
        view->setAction(action);
        view->setInhibitorList(inhibitors);

        switch (action) {
        case Actions::Shutdown:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from shutting down, and forcing shut down may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case Actions::SwitchSystem:
        case Actions::Restart:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from reboot, and forcing reboot may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case Actions::Suspend:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from suspend, and forcing suspend may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case Actions::Hibernate:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from hibernate, and forcing hibernate may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case Actions::Logout:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from log out, and forcing log out may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        default:
            return {};
        }


        bool isAccept = true;
        for (auto inhib : inhibitors) {
            if (inhib.mode.compare("block") == 0) {
                isAccept = false;
                break;
            }
        }

        if (action == Shutdown) {
            view->setAcceptReason(tr("Shut down"));
            view->setAcceptVisible(isAccept);
        } else if (action == Restart || action == SwitchSystem) {
            view->setAcceptReason(tr("Reboot"));
            view->setAcceptVisible(isAccept);
        } else if (action == Suspend)
            view->setAcceptReason(tr("Suspend"));
        else if (action == Hibernate)
            view->setAcceptReason(tr("Hibernate"));
        else if (action == Logout) {
            view->setAcceptReason(tr("Log out"));
            view->setAcceptVisible(isAccept);
        }

        m_warningView = view;
        m_mainLayout->addWidget(m_warningView);
        m_mainLayout->setCurrentWidget(m_warningView);

        connect(view, &InhibitWarnView::cancelled, this, &ContentWidget::onCancel);
        connect(view, &InhibitWarnView::actionInvoked, [this, action] {
            shutDownFrameActions(action);
        });

        return false;
    }

    if (loginUsers.length() > 1 && (action == Shutdown || action == Restart)) {
        QList<std::shared_ptr<User>> tmpList = loginUsers;

        for (auto it = tmpList.begin(); it != tmpList.end();) {
            if ((*it)->uid() == m_model->currentUser()->uid()) {
                it = tmpList.erase(it);
                continue;
            }
            ++it;
        }

        MultiUsersWarningView *view = new MultiUsersWarningView(this);
        view->setUsers(tmpList);
        view->setAction(action);
        if (action == Shutdown)
            view->setAcceptReason(tr("Shut down"));
        else if (action == Restart)
            view->setAcceptReason(tr("Reboot"));

        m_warningView = view;
        m_mainLayout->addWidget(m_warningView);
        m_mainLayout->setCurrentWidget(m_warningView);

        connect(view, &MultiUsersWarningView::cancelled, this, &ContentWidget::onCancel);
        connect(view, &MultiUsersWarningView::actionInvoked, [this, action] {
            shutDownFrameActions(action);
        });

        return false;
    }

    if (m_confirm && (action == Shutdown || action == Restart || action == Logout)) {
        m_confirm = false;

        InhibitWarnView *view = new InhibitWarnView(action, this);
        view->setAction(action);
        if (action == Shutdown) {
            view->setAcceptReason(tr("Shut down"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to shut down?"));
        } else if (action == Restart) {
            view->setAcceptReason(tr("Reboot"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to reboot?"));
        } else if (action == Logout) {
            view->setAcceptReason(tr("Log out"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to log out?"));
        }

        m_warningView = view;

        m_mainLayout->addWidget(m_warningView);
        m_mainLayout->setCurrentWidget(m_warningView);

        connect(view, &InhibitWarnView::cancelled, this, &ContentWidget::onCancel);
        connect(view, &InhibitWarnView::actionInvoked, [this, action] {
            shutDownFrameActions(action);
        });

        return false;
    }

    shutDownFrameActions(action);

    return true;
}

void ContentWidget::hideToplevelWindow()
{
    QWidgetList widgets = qApp->topLevelWidgets();
    for (QWidget *widget : widgets) {
        if (widget->isVisible()) {
            widget->hide();
        }
    }
}

void ContentWidget::shutDownFrameActions(const Actions action)
{
#ifdef QT_DEBUG
    return;
#endif // QT_DEBUG
    // if we don't force this widget to hide, hideEvent will happen after
    // dde-lock showing, since hideEvent enables hot zone, hot zone will
    // take effect while dde-lock is showing.

    switch (action) {
    case Shutdown:       m_sessionInterface->RequestShutdown();      break;
    case Restart:        m_sessionInterface->RequestReboot();        break;
    case Suspend: {
        m_model->setIsShow(false);
        QTimer::singleShot(200, this, [ = ] {m_sessionInterface->RequestSuspend();});
        break;
    }
    case Hibernate: {
        m_model->setIsShow(false);
        QTimer::singleShot(200, this, [ = ] {m_sessionInterface->RequestHibernate();});
        break;
    }
    case Lock:           m_sessionInterface->RequestLock();          break;
    case Logout:         m_sessionInterface->RequestLogout();        break;
    case SwitchSystem: {
        m_switchosInterface->setOsFlag(!m_switchosInterface->getOsFlag());
        QTimer::singleShot(200, this, [ = ] { m_sessionInterface->RequestReboot(); });
        break;
    }
    case SwitchUser: {
        DDBusSender()
        .service("com.deepin.dde.lockFront")
        .interface("com.deepin.dde.lockFront")
        .path("/com/deepin/dde/lockFront")
        .method("ShowUserList")
        .call();
        break;
    }
    default:
        qWarning() << "action: " << action << " not handled";
        break;
    }

    hideToplevelWindow();
}

void ContentWidget::currentWorkspaceChanged()
{
    QDBusPendingCall call = m_wmInter->GetCurrentWorkspaceBackgroundForMonitor(QGuiApplication::primaryScreen()->name());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [ = ] {
        if (!call.isError())
        {
            QDBusReply<QString> reply = call.reply();
            updateWallpaper(reply.value());
        } else
        {
            qWarning() << "get current workspace background error: " << call.error().message();
            updateWallpaper("/usr/share/backgrounds/deepin/desktop.jpg");
        }

        watcher->deleteLater();
    });
}

void ContentWidget::updateWallpaper(const QString &path)
{
    const QUrl url(path);
    QString wallpaper = path;
    if (url.isLocalFile()) {
        wallpaper = url.path();
    }

    emit requestBackground(wallpaper);
}

void ContentWidget::onUserListChanged(QList<std::shared_ptr<User> > list)
{
    const bool allowShowUserSwitchButton = m_model->allowShowUserSwitchButton();
    const bool alwaysShowUserSwitchButton = m_model->alwaysShowUserSwitchButton();
    bool haveLogindUser = true;

    if (m_model->isServerModel() && m_model->currentType() == SessionBaseModel::LightdmType) {
        haveLogindUser = !m_model->logindUser().isEmpty();
    }
    m_switchUserBtn->setVisible((alwaysShowUserSwitchButton ||
                                          (allowShowUserSwitchButton &&
                                          (list.size() > (m_model->isServerModel() ? 0 : 1)))) &&
                                          haveLogindUser);
}

void ContentWidget::enableHibernateBtn(bool enable)
{
    m_hibernateButton->setVisible(enable);
    if (!enable) {
        hideBtn("Hibernate");
    }
}

void ContentWidget::enableSleepBtn(bool enable)
{
    m_suspendButton->setVisible(enable);
    if (!enable) {
        hideBtn("Suspend");
    }
}

void ContentWidget::initUI()
{
    m_btnsList = new QList<RoundItemButton *>;
    m_shutdownButton = new RoundItemButton(tr("Shut down"));
    m_shutdownButton->setAutoExclusive(true);
    m_shutdownButton->setObjectName("ShutDownButton");
    m_restartButton = new RoundItemButton(tr("Reboot"));
    m_restartButton->setAutoExclusive(true);
    m_restartButton->setObjectName("RestartButton");
    m_suspendButton = new RoundItemButton(tr("Suspend"));
    m_suspendButton->setAutoExclusive(true);
    m_suspendButton->setObjectName("SuspendButton");
    m_hibernateButton = new RoundItemButton(tr("Hibernate"));
    m_hibernateButton->setAutoExclusive(true);
    m_hibernateButton->setObjectName("HibernateButton");
    m_lockButton = new RoundItemButton(tr("Lock"));
    m_lockButton->setAutoExclusive(true);
    m_lockButton->setObjectName("LockButton");
    m_logoutButton = new RoundItemButton(tr("Log out"));
    m_logoutButton->setAutoExclusive(true);
    m_logoutButton->setObjectName("LogoutButton");

    m_switchUserBtn = new RoundItemButton(tr("Switch user"));
    m_switchUserBtn->setAutoExclusive(true);
    m_switchUserBtn->setObjectName("SwitchUserButton");

    if (m_switchosInterface->isDualOsSwitchAvail()) {
        m_switchSystemBtn = new RoundItemButton(tr("Switch system"));
        m_switchSystemBtn->setAutoExclusive(true);
        m_switchSystemBtn->setObjectName("SwitchSystemButton");
    }

    m_normalView = new QWidget(this);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QVBoxLayout *defaultpageLayout = new QVBoxLayout;
    buttonLayout->setMargin(0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_shutdownButton);
    buttonLayout->addWidget(m_restartButton);
    buttonLayout->addWidget(m_suspendButton);
    buttonLayout->addWidget(m_hibernateButton);
    buttonLayout->addWidget(m_lockButton);
    buttonLayout->addWidget(m_switchUserBtn);
    if(m_switchSystemBtn) buttonLayout->addWidget(m_switchSystemBtn);
    buttonLayout->addWidget(m_logoutButton);
    buttonLayout->addStretch(0);

    defaultpageLayout->setMargin(0);
#ifdef QT_DEBUG
    QLabel *m_debugHint = new QLabel("!!! DEBUG MODE !!!", this);
    m_debugHint->setStyleSheet("color: #FFF;");
    defaultpageLayout->addWidget(m_debugHint);
    defaultpageLayout->setAlignment(m_debugHint, Qt::AlignCenter);
#endif // QT_DEBUG
    defaultpageLayout->addSpacerItem(m_buttonSpacer = new QSpacerItem(0, 0));
    defaultpageLayout->addLayout(buttonLayout);
    defaultpageLayout->addStretch();
    m_normalView->setLayout(defaultpageLayout);

    m_mainLayout = new QStackedLayout;
    m_mainLayout->setMargin(0);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_normalView);

    m_inhibitorBlacklists << "NetworkManager" << "ModemManager" << "com.deepin.daemon.Power";

    if (findValueByQSettings<bool>(DDESESSIONCC::SHUTDOWN_CONFIGS, "Shutdown", "enableSystemMonitor", true)) {
        QFile file("/usr/bin/deepin-system-monitor");
        if (file.exists()) {
            m_systemMonitor = new SystemMonitor(m_normalView);
            defaultpageLayout->addWidget(m_systemMonitor);
            defaultpageLayout->setAlignment(m_systemMonitor, Qt::AlignCenter);
            defaultpageLayout->addSpacing(40);
        }
    }

    setFocusPolicy(Qt::StrongFocus);
    setLayout(m_mainLayout);

    updateStyle(":/skin/shutdown.qss", this);

    m_btnsList->append(m_shutdownButton);
    m_btnsList->append(m_restartButton);
    m_btnsList->append(m_suspendButton);
    m_btnsList->append(m_hibernateButton);
    m_btnsList->append(m_lockButton);
    m_btnsList->append(m_switchUserBtn);
    if(m_switchSystemBtn) m_btnsList->append(m_switchSystemBtn);
    m_btnsList->append(m_logoutButton);

    //// Inhibit to shutdown
    // blumia: seems this call is useless..
//    listInhibitors();

//    QTimer *checkTooltip = new QTimer(this);
//    checkTooltip->setInterval(10000);
//    checkTooltip->setSingleShot(false);
//    connect(checkTooltip,  &QTimer::timeout, [this] {
//        InhibitWarnView *view = qobject_cast<InhibitWarnView *>(m_warningView);
//        if (!view)
//            return;

//        QList<InhibitWarnView::InhibitorData> list = listInhibitors(view->inhibitType());
//        view->setInhibitorList(list);

//        if (list.isEmpty())
//        {
//            // 清空提示的内容
//            view->setInhibitConfirmMessage(QString());
//            view->setAcceptVisible(true);
//        }
//    });
//    checkTooltip->start();
}

void ContentWidget::initBackground()
{
    if (m_wmInter->isValid()) {
        currentWorkspaceChanged();
    } else {
        QTimer::singleShot(0, this, [ = ] {
            updateWallpaper(m_dbusAppearance->background());
        });
    }

    connect(m_dbusAppearance, &Appearance::Changed, this, [ = ](const QString & type, const QString & path) {
        if (type == "background") {
            updateWallpaper(path.split(";").first());
        }
    });
}

QList<InhibitWarnView::InhibitorData> ContentWidget::listInhibitors(const Actions action)
{
    QList<InhibitWarnView::InhibitorData> inhibitorList;

    if (m_login1Inter->isValid()) {
        qDebug() <<  "m_login1Inter is valid!";

        QDBusPendingReply<InhibitorsList> reply = m_login1Inter->ListInhibitors();
        reply.waitForFinished();

        if (!reply.isError()) {
            InhibitorsList inhibitList = qdbus_cast<InhibitorsList>(reply.argumentAt(0));
            qDebug() << "inhibitList:" << inhibitList.count();

            QString type;

            switch (action) {
            case Actions::Shutdown:
            case Actions::Restart:
            case Actions::SwitchSystem:
            case Actions::Logout:
                type = "shutdown";
                break;
            case Actions::Suspend:
            case Actions::Hibernate:
                type = "sleep";
                break;
            default:
                return {};
            }

            for (int i = 0; i < inhibitList.count(); i++) {
                // Just take care of DStore's inhibition, ignore others'.
                const Inhibit &inhibitor = inhibitList.at(i);
                if (inhibitor.what.split(':', QString::SkipEmptyParts).contains(type)
                        && !m_inhibitorBlacklists.contains(inhibitor.who)) {

                    // 待机时，非block暂不处理，因为目前没有倒计时待机功能
                    if (type == "sleep" && inhibitor.mode != "block")
                        continue;

                    InhibitWarnView::InhibitorData inhibitData;
                    inhibitData.who = inhibitor.who;
                    inhibitData.why = inhibitor.why;
                    inhibitData.mode = inhibitor.mode;
                    inhibitData.pid = inhibitor.pid;

                    // 读取翻译后的文本，读取应用图标
                    QDBusConnection connection = QDBusConnection::sessionBus();
                    if (!inhibitor.uid) {
                        connection = QDBusConnection::systemBus();
                    }

                    if (connection.interface()->isServiceRegistered(inhibitor.who)) {

                        QDBusInterface ifc(inhibitor.who, "/com/deepin/InhibitHint", "com.deepin.InhibitHint", connection);
                        QDBusMessage msg = ifc.call("Get", getenv("LANG"), inhibitor.why);
                        if (msg.type() == QDBusMessage::ReplyMessage) {
                            InhibitHint inhibitHint = qdbus_cast<InhibitHint>(msg.arguments().at(0).value<QDBusArgument>());

                            if (!inhibitHint.why.isEmpty()) {
                                inhibitData.who = inhibitHint.name;
                                inhibitData.why = inhibitHint.why;
                                inhibitData.icon = inhibitHint.icon;
                            }
                        }
                    }

                    inhibitorList.append(inhibitData);
                }
            }
//            showTips(reminder_tooltip);
            qDebug() << "List of valid '" << type << "' inhibitors:";

            for (const InhibitWarnView::InhibitorData &data : inhibitorList) {
                qDebug() << "who:" << data.who;
                qDebug() << "why:" << data.why;
                qDebug() << "pid:" << data.pid;
            }

            qDebug() << "End list inhibitor";
        } else {
            qWarning() << "D-Bus request reply error:" << reply.error().message();
        }
    } else {
        qWarning() << "shutdown login1Manager error!";
    }

    return inhibitorList;
}

void ContentWidget::recoveryLayout()
{
    for (RoundItemButton *btn : *m_btnsList) {
        btn->show();
    }

    // check hibernate
    enableHibernateBtn(m_model->hasSwap());

    // check sleep
    enableSleepBtn(m_model->canSleep());

    // check user switch button
    onUserListChanged(m_model->userList());

    if (m_warningView != nullptr) {
        m_mainLayout->removeWidget(m_warningView);
        m_warningView->deleteLater();
        m_warningView = nullptr;
    }

    m_mainLayout->setCurrentWidget(m_normalView);
}

void ContentWidget::runSystemMonitor()
{
    QProcess::startDetached("/usr/bin/deepin-system-monitor");

    if (m_systemMonitor) {
        m_systemMonitor->clearFocus();
        m_systemMonitor->setState(SystemMonitor::Leave);
    }

    hideToplevelWindow();
}

void ContentWidget::setPreviousChildFocus()
{
    if (m_warningView && m_warningView->isVisible()) return;

    if (!m_currentSelectedBtn->isDisabled() &&
            !m_currentSelectedBtn->isChecked())
        m_currentSelectedBtn->updateState(RoundItemButton::Normal);

    const int lastPos = m_btnsList->indexOf(m_currentSelectedBtn);
    const int nextPos = lastPos ? lastPos : m_btnsList->count();

    m_currentSelectedBtn = m_btnsList->at(nextPos - 1);

    if (m_currentSelectedBtn->isDisabled() || !m_currentSelectedBtn->isVisible())
        setPreviousChildFocus();
    else
        m_currentSelectedBtn->setChecked(true);
}

void ContentWidget::setNextChildFocus()
{
    if (m_warningView && m_warningView->isVisible()) return;

    if (!m_currentSelectedBtn->isDisabled() &&
            !m_currentSelectedBtn->isChecked())
        m_currentSelectedBtn->updateState(RoundItemButton::Normal);

    const int lastPos = m_btnsList->indexOf(m_currentSelectedBtn);
    m_currentSelectedBtn = m_btnsList->at((lastPos + 1) % m_btnsList->count());

    if (m_currentSelectedBtn->isDisabled() || !m_currentSelectedBtn->isVisible())
        setNextChildFocus();
    else
        m_currentSelectedBtn->setChecked(true);
}

void ContentWidget::showTips(const QString &tips)
{
    if (!tips.isEmpty()) {
//        m_tipsLabel->setText(tips);
        m_tipsWidget->show();
//        m_shutdownButton->setDisabled(true);
//        m_restartButton->setDisabled(true);
//        m_suspendButton->setDisabled(true);
//        m_logoutButton->setDisabled(true);
    } else {
        m_tipsWidget->hide();
        m_shutdownButton->setDisabled(false);
        m_restartButton->setDisabled(false);
        m_suspendButton->setDisabled(false);
        m_logoutButton->setDisabled(false);
        m_shutdownButton->updateState(RoundItemButton::Hover);
    }
}

void ContentWidget::hideBtns(const QStringList &btnsName)
{
    for (const QString &name : btnsName)
        hideBtn(name);
}

void ContentWidget::disableBtns(const QStringList &btnsName)
{
    for (const QString &name : btnsName)
        disableBtn(name);
}

void ContentWidget::onCancel()
{
    hideToplevelWindow();
}


void ContentWidget::tryGrabKeyboard()
{
#ifdef QT_DEBUG
    return;
#endif

    if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(true)) {
        m_failures = 0;
        return;
    }

    m_failures++;

    if (m_failures == 15) {
        qDebug() << "Trying grabkeyboard has exceeded the upper limit. dde-lock will quit.";
        qApp->quit();
        return;
    }

    QTimer::singleShot(100, this, &ContentWidget::tryGrabKeyboard);
}
