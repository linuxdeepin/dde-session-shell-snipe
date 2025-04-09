// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loginwindow.h"
#include "logincontent.h"
#include "userinfo.h"

#include <QWindow>
#include <QKeyEvent>
DCORE_USE_NAMESPACE

LoginWindow::LoginWindow(SessionBaseModel *const model, QWidget *parent)
    : FullScreenBackground(model, parent)
    , m_model(model)
    , m_dconfig(DConfig::create("org.deepin.dde.daemon", "org.deepin.dde.daemon.account", QString(), this))
{
    setAccessibleName("LoginWindow");
    setContent(LoginContent::instance());

    connect(LoginContent::instance(), &LockContent::requestBackground, this, [this](const QString & wallpaper) {
        updateBackground(wallpaper);
        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
    });

    connect(model, &SessionBaseModel::visibleChanged, this, &LoginWindow::setVisible);
    connect(model, &SessionBaseModel::authFinished, this, [this](bool successful) {
        setEnterEnable(!successful);

        // 在认证成功以后会通过更改背景来实现登录动画，但是禁用登录动画的情况下，会立即调用startSession，
        // 导致当前进程被lightdm退掉，X上会残留上一帧的画面，可以看到输入框等画面。使用repaint()强制刷新背景来避免这个问题。
        repaint();
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
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        // startdde屏蔽了systemd的待机，由greeter处理待机流程
        case Qt::Key_Sleep: {
             m_model->setPowerAction(SessionBaseModel::RequireSuspend);
             break;
        }
        case Qt::Key_PowerOff: {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
            break;
        }
        // win+p时,触发xrandr -q.用于规避联想机型GT730显卡热插拔问题
        case Qt::Key_P: {
            if (!qgetenv("XDG_SESSION_TYPE").contains("wayland")) {
                if (qApp->queryKeyboardModifiers().testFlag(Qt::MetaModifier)) {
                    auto ret = QProcess::execute("xrandr", QStringList() << "-q");
                    qCInfo(DDE_SHELL) << "Exec `xrandr -q` ret:" << ret;
                }
            }
            break;
        }
        case Qt::Key_Escape: {
            // TODO for test
            qCInfo(DDE_SHELL) << "loginWindow::keyPressEvent set terminal false";
            if (m_dconfig) {
                bool allowLocalUnlockTerminal = m_dconfig->value("isAllowLocalUnlockTerminal", false).toBool();
                qCInfo(DDE_SHELL) << "allowLocalUnlockTerminal : "<<allowLocalUnlockTerminal;
                if(allowLocalUnlockTerminal){
                    QProcess process;
#ifndef ENABLE_DSS_SNIPE
                    process.start("dbus-send --print-reply --system --dest=com.deepin.daemon.Accounts /com/deepin/daemon/Accounts com.deepin.daemon.Accounts.SetTerminalLocked boolean:false");
#else
                    process.setProgram("dbus-send");
                    process.setArguments( {"--print-reply", "--system", "--dest=org.deepin.dde.Accounts1", "/org/deepin/dde/Accounts1", "org.deepin.dde.Accounts1.SetTerminalLocked", "boolean:false"});
                    process.start();
#endif
                    process.waitForFinished();
                }
            }

            break;
        }
        default:
            break;
        }
    }
    return FullScreenBackground::event(event);
}
