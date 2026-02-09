// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfig_helper.h"
#include "warningcontent.h"
#include "fullscreenbackground.h"
#include "lockcontent.h"
#include "keyboardmonitor.h"

#include <DDBusSender>

#include "dbusconstant.h"

WarningContent::WarningContent(QWidget *parent)
    : SessionBaseWindow(parent)
    , m_model(nullptr)
    , m_login1Inter(new DBusLogin1Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this))
    , m_powerAction(SessionBaseModel::PowerAction::None)
    , m_failures(0)
{
    setAccessibleName("WarningContent");
    m_inhibitorBlacklists << "NetworkManager" << "ModemManager" << DSS_DBUS::sessionPowerService;
    setTopFrameVisible(false);
    setBottomFrameVisible(false);
}

WarningContent::~WarningContent()
{

}

WarningContent *WarningContent::instance()
{
    static WarningContent *warningContent = nullptr;
    if (!warningContent) {
        warningContent = new WarningContent;
    }

    return warningContent;
}

void WarningContent::setModel(SessionBaseModel * const model)
{
    m_model = model;
    connect(m_model, &SessionBaseModel::shutdownInhibit, this, &WarningContent::shutdownInhibit);
}

QList<InhibitWarnView::InhibitorData> WarningContent::listInhibitors(const SessionBaseModel::PowerAction action)
{
    std::shared_ptr<User> currentUser = m_model->currentUser();
    if (!currentUser)
        return QList<InhibitWarnView::InhibitorData>();

    QList<InhibitWarnView::InhibitorData> inhibitorList;

    if (m_login1Inter->isValid()) {
        QDBusPendingReply<InhibitorsList> reply = m_login1Inter->ListInhibitors();
        reply.waitForFinished();

        if (!reply.isError()) {
            InhibitorsList inhibitList = qdbus_cast<InhibitorsList>(reply.argumentAt(0));
            qCDebug(DDE_SHELL) << "Inhibitors list count: " << inhibitList.count();

            QString type;

            switch (action) {
            case SessionBaseModel::PowerAction::RequireShutdown:
            case SessionBaseModel::PowerAction::RequireUpdateShutdown:
            case SessionBaseModel::PowerAction::RequireRestart:
            case SessionBaseModel::PowerAction::RequireUpdateRestart:
            case SessionBaseModel::PowerAction::RequireSwitchSystem:
            case SessionBaseModel::PowerAction::RequireLogout:
                type = "shutdown";
                break;
            case SessionBaseModel::PowerAction::RequireSuspend:
            case SessionBaseModel::PowerAction::RequireHibernate:
                type = "sleep";
                break;
            default:
                return {};
            }

            QStringList delayInhibitIgnoreList = DConfigHelper::instance()->getConfig("delayInhibitIgnoreList", QStringList()).toStringList();

            for (int i = 0; i < inhibitList.count(); i++) {
                // Just take care of DStore's inhibition, ignore others'.
                const Inhibit &inhibitor = inhibitList.at(i);
                if (inhibitor.uid != currentUser->uid() && inhibitor.uid != 0)
                    continue;

#ifndef ENABLE_DSS_SNIPE
                if (inhibitor.what.split(':', QString::SkipEmptyParts).contains(type)
#else
                if (inhibitor.what.split(':', Qt::SkipEmptyParts).contains(type)
#endif
                        && !m_inhibitorBlacklists.contains(inhibitor.who)) {

                    // 待机时，非block暂不处理，因为目前没有倒计时待机功能
                    if (type == "sleep" && inhibitor.mode != "block")
                        continue;

                    if (inhibitor.mode == "delay" && delayInhibitIgnoreList.contains(inhibitor.who))
                        continue;

                    InhibitWarnView::InhibitorData inhibitData;
                    inhibitData.who = inhibitor.who;
                    inhibitData.why = inhibitor.why;
                    inhibitData.mode = inhibitor.mode;
                    inhibitData.pid = inhibitor.pid;

                    if(action == SessionBaseModel::PowerAction::RequireLogout && inhibitor.uid != m_model->currentUser()->uid())
                        continue;

                    // 读取翻译后的文本，读取应用图标
                    QDBusConnection connection = QDBusConnection::sessionBus();
                    if (!inhibitor.uid) {
                        connection = QDBusConnection::systemBus();
                    }

                    if (connection.interface()->isServiceRegistered(inhibitor.who)) {

                        QDBusInterface ifc(inhibitor.who, DSS_DBUS::inhibitHintPath, DSS_DBUS::inhibitHintService, connection);
                        QDBusMessage msg = ifc.call("Get", qgetenv("LANG"), inhibitor.why);
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

            for (const InhibitWarnView::InhibitorData &data : inhibitorList) {
                qCDebug(DDE_SHELL) << "Inhibitor list detail: who:" << data.who
                         << ", why:" << data.why
                         << ", pid:" << data.pid;
            }

        } else {
            qCWarning(DDE_SHELL) << "DBus request reply error:" << reply.error().message();
        }
    } else {
        qCWarning(DDE_SHELL) <<  "Login1 interface is invalid";
    }

    return inhibitorList;
}

void WarningContent::doCancelShutdownInhibit()
{
    InhibitWarnView *view = qobject_cast<InhibitWarnView *>(m_warningView);
    if (view && view->delayView())
        return;

    qCInfo(DDE_SHELL) << "Cancel shutdown inhibit";
    m_model->setPowerAction(SessionBaseModel::PowerAction::None);
    FullScreenBackground::setContent(LockContent::instance());
    m_model->setCurrentContentType(SessionBaseModel::LockContent);
    // 从lock点的power btn，不能隐藏锁屏而进入桌面
    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
        m_model->setVisible(false);
        m_model->setCurrentModeState(SessionBaseModel::PasswordMode);
    }
}

void WarningContent::doAcceptShutdownInhibit()
{
    qCInfo(DDE_SHELL) << "Accept shutdown inhibit, power action: " << m_powerAction
            << ", current mode: " << m_model->currentModeState();
    InhibitWarnView *view = qobject_cast<InhibitWarnView *>(sender());
    if (view) {
        view->setDelayView(false);
        if (view->hasInhibit()) {
            switch (m_powerAction) {
            case SessionBaseModel::PowerAction::RequireShutdown:
            case SessionBaseModel::PowerAction::RequireUpdateShutdown:
                view->setDelayView(true);
                view->setInhibitConfirmMessage(tr("Closing the programs and shutting down, please wait..."), true);
                break;
            case SessionBaseModel::PowerAction::RequireRestart:
            case SessionBaseModel::PowerAction::RequireUpdateRestart:
                view->setDelayView(true);
                view->setInhibitConfirmMessage(tr("Closing the programs and rebooting, please wait..."), true);
                break;
            case SessionBaseModel::PowerAction::RequireLogout:
                view->setDelayView(true);
                view->setInhibitConfirmMessage(tr("Closing the programs and logging out, please wait..."), true);
                break;
            default:
                break;
            }
        }
    }

    m_model->setPowerAction(m_powerAction);

    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode
        && m_model->currentModeState() != SessionBaseModel::ModeStatus::PowerMode
        && m_powerAction != SessionBaseModel::RequireUpdateShutdown
        && m_powerAction != SessionBaseModel::RequireUpdateRestart
        && m_powerAction != SessionBaseModel::RequireShutdown
        && m_powerAction != SessionBaseModel::RequireRestart ) {
        FullScreenBackground::setContent(LockContent::instance());
        m_model->setCurrentContentType(SessionBaseModel::LockContent);
    }
}

void WarningContent::beforeInvokeAction(bool needConfirm)
{
    const QList<InhibitWarnView::InhibitorData> inhibitors = listInhibitors(m_powerAction);
    const QList<std::shared_ptr<User>> &loginUsers = m_model->loginedUserList();

    if (m_warningView != nullptr) {
        qCInfo(DDE_SHELL) << "Before invoke action, delete warning view: " << m_warningView;
        m_warningView->deleteLater();
        m_warningView = nullptr;
    }

    // change ui
    if (!inhibitors.isEmpty()) {
        InhibitWarnView *view = new InhibitWarnView(m_powerAction, this);
        view->setInhibitorList(inhibitors);

        switch (m_powerAction) {
        case SessionBaseModel::PowerAction::RequireShutdown:
        case SessionBaseModel::PowerAction::RequireUpdateShutdown:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from shutting down, and forcing shut down may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case SessionBaseModel::PowerAction::RequireSwitchSystem:
        case SessionBaseModel::PowerAction::RequireRestart:
        case SessionBaseModel::PowerAction::RequireUpdateRestart:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from reboot, and forcing reboot may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case SessionBaseModel::PowerAction::RequireSuspend:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from suspend, and forcing suspend may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case SessionBaseModel::PowerAction::RequireHibernate:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from hibernate, and forcing hibernate may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        case SessionBaseModel::PowerAction::RequireLogout:
            view->setInhibitConfirmMessage(tr("The programs are preventing the computer from log out, and forcing log out may cause data loss.") + "\n" +
                                           tr("To close the program, click Cancel, and then close the program."));
            break;
        default:
            return;
        }

        // 如果有阻止关机、重启、待机或休眠的进程，则不允许手动强制执行
        bool isBlock = std::any_of(inhibitors.begin(), inhibitors.end(),
                                    [](const InhibitWarnView::InhibitorData &inhib) { return inhib.mode.compare("block") == 0; });

        if (m_powerAction == SessionBaseModel::PowerAction::RequireShutdown) {
            view->setAcceptReason(tr("Shut down"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireRestart
            || m_powerAction == SessionBaseModel::PowerAction::RequireSwitchSystem) {
            view->setAcceptReason(tr("Reboot"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireSuspend) {
            view->setAcceptReason(tr("Suspend"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireHibernate) {
            view->setAcceptReason(tr("Hibernate"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireLogout) {
            view->setAcceptReason(tr("Log out"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireUpdateShutdown) {
            view->setAcceptReason(tr("Update and Shut Down"));
            view->setAcceptVisible(!isBlock);
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireUpdateRestart) {
            view->setAcceptReason(tr("Update and Reboot"));
            view->setAcceptVisible(!isBlock);
        }

        m_warningView = view;
        setCenterContent(m_warningView);

        qCInfo(DDE_SHELL) << "Before invoke action, warning view: " << m_warningView;

        connect(view, &InhibitWarnView::cancelled, this, &WarningContent::doCancelShutdownInhibit);
        connect(view, &InhibitWarnView::actionInvoked, this, &WarningContent::doAcceptShutdownInhibit);

        return;
    }

    if (loginUsers.length() > 1
        && (m_powerAction == SessionBaseModel::PowerAction::RequireShutdown
        || m_powerAction == SessionBaseModel::PowerAction::RequireRestart
        || m_powerAction == SessionBaseModel::PowerAction::RequireUpdateShutdown
        || m_powerAction == SessionBaseModel::PowerAction::RequireUpdateRestart)) {
        QList<std::shared_ptr<User>> tmpList = loginUsers;

        for (auto it = tmpList.begin(); it != tmpList.end();) {
            if ((*it)->uid() == m_model->currentUser()->uid()) {
                it = tmpList.erase(it);
                continue;
            }
            ++it;
        }

        MultiUsersWarningView *view = new MultiUsersWarningView(m_powerAction, this);
        view->setUsers(tmpList);
        if (m_powerAction == SessionBaseModel::PowerAction::RequireShutdown)
            view->setAcceptReason(tr("Shut down"));
        else if (m_powerAction == SessionBaseModel::PowerAction::RequireRestart)
            view->setAcceptReason(tr("Reboot"));
        else if (m_powerAction == SessionBaseModel::PowerAction::RequireUpdateRestart)
            view->setAcceptReason(tr("Update and Reboot"));
        else if (m_powerAction == SessionBaseModel::PowerAction::RequireUpdateShutdown)
            view->setAcceptReason(tr("Update and Shut Down"));

        m_warningView = view;
        m_warningView->setFixedSize(getCenterContentSize());
        setCenterContent(m_warningView);

        connect(view, &MultiUsersWarningView::cancelled, this, &WarningContent::doCancelShutdownInhibit);
        connect(view, &MultiUsersWarningView::actionInvoked, this, &WarningContent::doAcceptShutdownInhibit);

        return;
    }

    if (needConfirm && (m_powerAction == SessionBaseModel::PowerAction::RequireShutdown ||
                        m_powerAction == SessionBaseModel::PowerAction::RequireRestart ||
                        m_powerAction == SessionBaseModel::PowerAction::RequireLogout)) {
        InhibitWarnView *view = new InhibitWarnView(m_powerAction, this);
        if (m_powerAction == SessionBaseModel::PowerAction::RequireShutdown
            || m_powerAction == SessionBaseModel::PowerAction::RequireUpdateShutdown) {
            view->setAcceptReason(tr("Shut down"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to shut down?"));
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireRestart
            || m_powerAction == SessionBaseModel::PowerAction::RequireUpdateRestart) {
            view->setAcceptReason(tr("Reboot"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to reboot?"));
        } else if (m_powerAction == SessionBaseModel::PowerAction::RequireLogout) {
            view->setAcceptReason(tr("Log out"));
            view->setInhibitConfirmMessage(tr("Are you sure you want to log out?"));
        }

        m_warningView = view;
        m_warningView->setFixedSize(getCenterContentSize());
        setCenterContent(m_warningView);

        connect(view, &InhibitWarnView::cancelled, this, &WarningContent::doCancelShutdownInhibit);
        connect(view, &InhibitWarnView::actionInvoked, this, &WarningContent::doAcceptShutdownInhibit);

        return;
    }

    doAcceptShutdownInhibit();
}

/**
 * @brief 抓取键盘,失败后继续抓取,最多尝试15次.
 *
 * @param exitIfFailed true: 发送失败通知，并隐藏锁屏。 false：不做任何处理。
 */
void WarningContent::tryGrabKeyboard(bool exitIfFailed)
{
#ifndef QT_DEBUG
    if (!isVisible()) {
        return;
    }

    if (m_model->isUseWayland()) {
        static QDBusInterface *kwinInter = new QDBusInterface("org.kde.KWin","/KWin","org.kde.KWin", QDBusConnection::sessionBus());
        if (!kwinInter || !kwinInter->isValid()) {
            qCWarning(DDE_SHELL) << "Kwin interface is invalid";
            m_failures = 0;
            return;
        }
        // wayland下判断是否有应用发起grab，如果有就不锁屏
        QDBusReply<bool> reply = kwinInter->call("xwaylandGrabed");
        if (!reply.isValid() || !reply.value()) {
            m_failures = 0;
            return;
        }
    } else {
        if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(true)) {
            m_failures = 0;
            return;
        }
    }

    if (m_failures > 5) {
        qCWarning(DDE_SHELL) << "Trying ungrab keyboard in warning content";
        KeyboardMonitor::instance()->ungrabKeyboard();
    }

    m_failures++;

    if (m_failures == 15) {
        qCWarning(DDE_SHELL) << "Trying to grab keyboard has exceeded the upper limit. dde-lock will quit.";

        m_failures = 0;

        if (!exitIfFailed) {
            return;
        }

        qCInfo(DDE_SHELL) << "Request hide lock frame";
        emit requestLockFrameHide();
        return;
    }

    QTimer::singleShot(100, this, [this, exitIfFailed] {
        tryGrabKeyboard(exitIfFailed);
    });
#endif
}

void WarningContent::showEvent(QShowEvent *event)
{
    tryGrabKeyboard();
    QFrame::showEvent(event);
}

void WarningContent::setPowerAction(const SessionBaseModel::PowerAction action)
{
    if (m_powerAction == action)
        return;

    m_powerAction = action;
}

bool WarningContent::supportDelayOrWait() const
{
    InhibitWarnView *view = qobject_cast<InhibitWarnView *>(m_warningView);
    return (view && view->hasInhibit() && view->delayView());
}

void WarningContent::mouseReleaseEvent(QMouseEvent *event)
{
    doCancelShutdownInhibit();
    return SessionBaseWindow::mouseReleaseEvent(event);
}

void WarningContent::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        doCancelShutdownInhibit();
        break;
    }
    QWidget::keyPressEvent(event);
}

void WarningContent::shutdownInhibit(const SessionBaseModel::PowerAction action, bool needConfirm)
{
    qCInfo(DDE_SHELL) << "Shutdown inhibit, action: " << action;
    setPowerAction(action);
    //检查是否允许关机
    FullScreenBackground::setContent(WarningContent::instance());
    m_model->setCurrentContentType(SessionBaseModel::WarningContent);
    beforeInvokeAction(needConfirm);
}
