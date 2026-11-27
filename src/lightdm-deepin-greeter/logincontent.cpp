// SPDX-FileCopyrightText: 2011 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logincontent.h"
#include "logintipswindow.h"
#include "sessionwidget.h"
#include "controlwidget.h"
#include "dconfig_helper.h"

#include <QDir>
#include <QProcess>
#include <QScopedValueRollback>

LoginContent::LoginContent(QWidget *parent)
    : LockContent(parent)
    , m_usersWithTheSameName(nullptr)
{

}

LoginContent *LoginContent::instance()
{
    static LoginContent* loginContent = nullptr;
    if (!loginContent) {
        loginContent = new LoginContent;
    }

    return loginContent;
}

void LoginContent::init(SessionBaseModel *model)
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    LockContent::init(model);
    setAccessibleName("LoginContent");

    m_sessionFrame = new SessionWidget;
    m_sessionFrame->setModel(model);
    m_controlWidget->setSessionSwitchEnable(m_sessionFrame->sessionCount() > 1);
    m_controlWidget->chooseToSession(model->sessionKey());

    m_loginTipsWindow = new LoginTipsWindow;
    connect(m_loginTipsWindow, &LoginTipsWindow::requestClosed, m_model, &SessionBaseModel::tipsShowed);

    connect(m_sessionFrame, &SessionWidget::notifyHideSessionFrame, this, &LockContent::restoreMode);
    connect(m_sessionFrame, &SessionWidget::sessionChanged, this, &LockContent::restoreMode);
    connect(m_controlWidget, &ControlWidget::requestSwitchSession, this, [this] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::SessionMode);
    });
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, m_controlWidget, &ControlWidget::chooseToSession);
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, this, &LockContent::restoreMode);
    connect(m_model, &SessionBaseModel::tipsShowed, this, &LoginContent::popTipsFrame);
}

void LoginContent::onSwitchUserRequested()
{
#ifdef ENABLE_DSS_SNIPE
    if (beginAccountEntry())
        return;
#endif
    LockContent::onSwitchUserRequested();
}

void LoginContent::setVisible(bool visible)
{
    // FullScreenBackground can show the shared content again while moving it between screens.
    LockContent::setVisible(visible && !accountEntryBlocksLogin());
}

bool LoginContent::usesAccountEntry() const
{
    if (m_model->userlistVisible() || !m_model->findUserByName(QStringLiteral("...")))
        return false;

    const QString shellConfig = QStringLiteral("org.deepin.dde.session-shell");
    return DConfigHelper::instance()->getConfig(shellConfig, shellConfig, {},
                                               QStringLiteral("loginPromptInput"), false).toBool();
}

bool LoginContent::beginAccountEntry()
{
    if (!m_greeterActive || m_taskProcess || m_startingAccountEntry)
        return true;

    if (!usesAccountEntry())
        return false;

    const QDir configs(QStringLiteral("/usr/share/dde-session-shell/greeters.d/account-entry.conf.d"));
    if (configs.entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty()) {
        enterAccountInput();
        emit accountEntryVisibilityChanged();
    } else {
        startAccountEntryTask(QStringLiteral("/usr/share/dde-session-shell/greeters.d/select-account-entry-task"), true);
    }
    return true;
}

void LoginContent::enterAccountInput()
{
    const auto accountEntryUser = m_model->findUserByName(QStringLiteral("..."));
    if (!accountEntryUser)
        return;

    QScopedValueRollback<bool> starting(m_startingAccountEntry, true);
    if (m_model->currentUser() != accountEntryUser)
        emit requestSwitchToUser(accountEntryUser);
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
    if (m_showTipsWidget && m_loginTipsWindow->isValid())
        pushTipsFrame();
    else
        pushPasswordFrame();
}

void LoginContent::reactivateAccountEntry()
{
    m_greeterActive = true;
    if (m_taskProcess)
        m_accountEntryPending = true;
    else if (beginAccountEntry())
        return;
    emit accountEntryVisibilityChanged();
}

void LoginContent::setGreeterActive(bool active)
{
    m_greeterActive = active;
    if (!active) {
        m_accountEntryPending = false;
        if (m_taskProcess && m_taskProcess->processChannelMode() == QProcess::ForwardedChannels)
            m_taskProcess->kill();
    }
    emit accountEntryVisibilityChanged();
}

void LoginContent::startAccountEntryTask(const QString &program, bool selecting)
{
    auto *process = new QProcess(this);
    process->setProgram(program);
    process->setProcessChannelMode(selecting ? QProcess::ForwardedErrorChannel : QProcess::ForwardedChannels);
    m_taskProcess = process;

    const auto finish = [this, process, selecting](int code, QProcess::ExitStatus status) {
        m_taskProcess = nullptr;
        process->deleteLater();
        if (m_greeterActive && m_accountEntryPending) {
            m_accountEntryPending = false;
            if (!beginAccountEntry())
                emit accountEntryVisibilityChanged();
            return;
        } else if (m_greeterActive && selecting && status == QProcess::NormalExit && code == 0) {
            const QString task = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
            if (QDir::isAbsolutePath(task)) {
                startAccountEntryTask(task, false);
                return;
            }
        }

        if (m_greeterActive)
            enterAccountInput();
        emit accountEntryVisibilityChanged();
    };
    connect(process, &QProcess::finished, this, [process, selecting, finish](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code > (selecting ? 1 : 0))
            qCWarning(DDE_SHELL) << "Account entry process exited" << process->program() << status << code;
        finish(code, status);
    });
    connect(process, &QProcess::errorOccurred, this, [process, finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(DDE_SHELL) << "Failed to start account entry process:" << process->program() << process->errorString();
            finish(-1, QProcess::CrashExit);
        }
    });

    emit accountEntryVisibilityChanged();
    process->start();
}

void LoginContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (!user.get())
        return;

    LockContent::onCurrentUserChanged(user);
    m_sessionFrame->switchToUser(user->name());
}

void LoginContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    if (m_taskProcess)
        return;

    qCInfo(DDE_SHELL) << "Status changed, status" << status << ", current status: " << m_currentModeStatus;
    switch (status) {
    case SessionBaseModel::ModeStatus::SessionMode:
        if (m_currentModeStatus == status)
            break;

        m_currentModeStatus = status;
        pushSessionFrame();
        break;
    default:
        pushTipsFrame();
        break;
    }
}

void LoginContent::pushSessionFrame()
{
    QSize size = getCenterContentSize();
    m_sessionFrame->setFixedSize(size);
    setCenterContent(m_sessionFrame);
}

void LoginContent::pushTipsFrame()
{
    // 如果配置文件存在，并且获取到的内容有效，则显示提示界面，并保证只在greeter启动时显示一次
    if (!m_model->passwordChangePageVisible() && m_showTipsWidget && m_loginTipsWindow->isValid()) {
        setTopFrameVisible(false);
        setBottomFrameVisible(false);
        QSize size = getCenterContentSize();
        m_loginTipsWindow->setFixedSize(size);
        setCenterContent(m_loginTipsWindow);
    } else {
        LockContent::onStatusChanged(m_model->currentModeState());
    }
}

void LoginContent::popTipsFrame()
{
    // 点击确认按钮后显示登录界面
    m_showTipsWidget = false;
    setTopFrameVisible(true);
    setBottomFrameVisible(true);
    LockContent::onStatusChanged(m_model->currentModeState());
}

void LoginContent::showUsersWithTheSameName(const QString &nativeUserName, const QString &doMainAccountDetail)
{
    if (!m_usersWithTheSameName) {
        m_usersWithTheSameName = new UsersWithTheSameName(m_model);
        connect(m_usersWithTheSameName, &UsersWithTheSameName::requestCheckAccount, this, &LoginContent::requestCheckAccount);
        connect(m_usersWithTheSameName, &UsersWithTheSameName::clicked, this, &LoginContent::restoreMode);
    }

    if (!m_usersWithTheSameName->findUsers(nativeUserName, doMainAccountDetail))
        return;
    m_model->setCurrentModeState(SessionBaseModel::SelectSameNameUserMode);
    QSize size = getCenterContentSize();
    m_usersWithTheSameName->setFixedSize(size);
    setCenterContent(m_usersWithTheSameName);
}
