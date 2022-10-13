// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loginwindow.h"
#include "logincontent.h"
#include "userinfo.h"

#include <QWindow>
#include <QKeyEvent>


LoginWindow::LoginWindow(SessionBaseModel *const model, QWidget *parent)
    : FullScreenBackground(model, parent)
    , m_model(model)
{
    setAccessibleName("LoginWindow");
    updateBackground(m_model->currentUser()->greeterBackground());
    setContent(LoginContent::instance());

    connect(LoginContent::instance(), &LockContent::requestBackground, this, [ this ](const QString & wallpaper) {
        updateBackground(wallpaper);
#ifdef DISABLE_LOGIN_ANI
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
#endif
    });

    connect(model, &SessionBaseModel::visibleChanged, this, &LoginWindow::setVisible);
    connect(model, &SessionBaseModel::authFinished, this, [ this ] (bool successful) {
        setEnterEnable(!successful);
#ifdef DISABLE_LOGIN_ANI
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
#endif
    });
}

LoginWindow::~LoginWindow()
{
    // 防止对象析构的时候把多屏共享的content对象也析构了
    if (LoginContent::instance()->parent() == this) {
        LoginContent::instance()->setParent(nullptr);
    }
}

void LoginWindow::showEvent(QShowEvent *event)
{
    FullScreenBackground::showEvent(event);

    //greeter界面显示时，需要调用虚拟键盘
    m_model->setHasVirtualKB(true);
}

void LoginWindow::hideEvent(QHideEvent *event)
{
    FullScreenBackground::hideEvent(event);

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
    return FullScreenBackground::event(event);
}
