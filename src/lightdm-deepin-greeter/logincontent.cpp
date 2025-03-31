// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logincontent.h"
#include "logintipswindow.h"
#include "sessionwidget.h"
#include "controlwidget.h"

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

void LoginContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (!user.get())
        return;

    LockContent::onCurrentUserChanged(user);
    m_sessionFrame->switchToUser(user->name());
}

void LoginContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
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
    if (m_showTipsWidget && m_loginTipsWindow->isValid()) {
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
