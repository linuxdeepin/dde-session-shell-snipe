/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#include "loginwindow.h"
#include "logincontent.h"
#include "userinfo.h"

#include <QWindow>

LoginWindow::LoginWindow(SessionBaseModel *const model, QWidget *parent)
    : FullscreenBackground(parent)
    , m_loginContent(new LoginContent(model, this))
    , m_model(model)
{
    QPixmap image(this->size());
    image.fill(Qt::black);
    updateBackground(image);

    QTimer::singleShot(0, this, [ = ] {
        auto user = model->currentUser();
        //默认刷新清晰的锁屏背景，避免因为获取虚化背景过慢而引起的白屏问题
        if (user != nullptr) updateBackground(QPixmap(user->greeterBackgroundPath()));
    });

    setContent(m_loginContent);
    m_loginContent->hide();

    connect(m_loginContent, &LockContent::requestBackground, this, [ = ](const QString & wallpaper) {
        updateBackground(wallpaper);
#ifdef DISABLE_LOGIN_ANI
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
#endif
    });

    connect(model, &SessionBaseModel::authFinished, this, [ = ](bool successd) {
        enableEnterEvent(!successd);
        if (successd) {
            m_loginContent->setVisible(!successd);
        }

#ifdef DISABLE_LOGIN_ANI
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
#endif
    });

    connect(m_loginContent, &LockContent::requestAuthUser, this, &LoginWindow::requestAuthUser);
    connect(m_loginContent, &LockContent::requestSwitchToUser, this, &LoginWindow::requestSwitchToUser);
    connect(m_loginContent, &LockContent::requestSetLayout, this, &LoginWindow::requestSetLayout);
    connect(m_loginContent, &LockContent::requestCheckAccountName, this, &LoginWindow::requestCheckAccountName);
}

void LoginWindow::resizeEvent(QResizeEvent *event)
{
    return FullscreenBackground::resizeEvent(event);
}

void LoginWindow::showEvent(QShowEvent *event)
{
    //greeter界面显示时，需要调用虚拟键盘
    FullscreenBackground::showEvent(event);

    m_model->setHasVirtualKB(true);
}

void LoginWindow::hideEvent(QHideEvent *event)
{
    //greeter界面隐藏时，需要结束虚拟键盘
    FullscreenBackground::hideEvent(event);

    m_model->setHasVirtualKB(false);
}
