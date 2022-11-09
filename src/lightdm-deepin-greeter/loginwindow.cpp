// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loginwindow.h"
#include "logincontent.h"
#include "userinfo.h"

#include <QWindow>
#include <QKeyEvent>

LoginWindow::LoginWindow(SessionBaseModel *const model, QWidget *parent)
    : FullscreenBackground(model, parent)
    , m_loginContent(new LoginContent(model, this))
    , m_model(model)
{
    setAccessibleName("LoginWindow");
    updateBackground(m_model->currentUser()->greeterBackground());
    setContent(m_loginContent);
    m_loginContent->setVisible(false);

    connect(m_loginContent, &LockContent::requestBackground, this, [ = ](const QString & wallpaper) {
        updateBackground(wallpaper);
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
    });

    connect(model, &SessionBaseModel::visibleChanged, this, &LoginWindow::setVisible);
    connect(model, &SessionBaseModel::authFinished, this, [ = ](bool successful) {
        setEnterEnable(!successful);
        if (successful)
            m_loginContent->setVisible(false);

        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
    });

    connect(m_loginContent, &LockContent::requestSwitchToUser, this, &LoginWindow::requestSwitchToUser);
    connect(m_loginContent, &LockContent::requestSetLayout, this, &LoginWindow::requestSetKeyboardLayout);

    connect(m_loginContent, &LockContent::requestCheckAccount, this, &LoginWindow::requestCheckAccount);
    connect(m_loginContent, &LockContent::requestStartAuthentication, this, &LoginWindow::requestStartAuthentication);
    connect(m_loginContent, &LockContent::sendTokenToAuth, this, &LoginWindow::sendTokenToAuth);
    connect(m_loginContent, &LockContent::requestEndAuthentication, this, &LoginWindow::requestEndAuthentication);
    connect(m_loginContent, &LockContent::authFinished, this, &LoginWindow::authFinished);
}

void LoginWindow::showEvent(QShowEvent *event)
{
    FullscreenBackground::showEvent(event);

    //greeter界面显示时，需要调用虚拟键盘
    m_model->setHasVirtualKB(true);
}

void LoginWindow::hideEvent(QHideEvent *event)
{
    FullscreenBackground::hideEvent(event);

    //greeter界面隐藏时，需要结束虚拟键盘
    m_model->setHasVirtualKB(false);
}

bool LoginWindow::event(QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        // startdde屏蔽了systemd的待机，由greeter处理待机流程
        case Qt::Key_Sleep: {
             m_model->setPowerAction(SessionBaseModel::RequireSuspend);
             break;
        }
        default:
            break;
        }
    }
    return FullscreenBackground::event(event);
}
