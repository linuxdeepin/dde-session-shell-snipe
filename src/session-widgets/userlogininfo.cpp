/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lixin <lixin_cm@deepin.com>
 *
 * Maintainer: lixin <lixin_cm@deepin.com>
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

#include "userlogininfo.h"
#include "userinfo.h"
#include "userloginwidget.h"
#include "sessionbasemodel.h"
#include "userframelist.h"
#include "constants.h"
#include <QKeyEvent>

/**
 * 定位：UserLoginWidget、UserFrameList的后端，托管它们需要用的数据
 * 只发送必要的信号（当有多个不同的信号过来，但是会触发显示同一个界面，这里做过滤），改变子控件的布局
 */
UserLoginInfo::UserLoginInfo(SessionBaseModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_userLoginWidget(new UserLoginWidget(model))
    , m_userFrameList(new UserFrameList)
{
    m_userFrameList->setModel(model);
    /* 初始化验证界面 */
    m_userLoginWidget->updateWidgetShowType(model->getAuthProperty().AuthType);
    m_userLoginWidget->updateLimitsInfo(m_model->currentUser()->limitsInfo());
    m_userLoginWidget->setAccessibleName("UserLoginWidget");
    initConnect();
}

void UserLoginInfo::setUser(std::shared_ptr<User> user)
{
    for (auto connect : m_currentUserConnects) {
        m_user->disconnect(connect);
    }

    m_currentUserConnects.clear();
    m_currentUserConnects << connect(user.get(), &User::limitsInfoChanged, m_userLoginWidget, &UserLoginWidget::updateLimitsInfo);
    m_currentUserConnects << connect(user.get(), &User::avatarChanged, m_userLoginWidget, &UserLoginWidget::updateAvatar);
    m_currentUserConnects << connect(user.get(), &User::displayNameChanged, m_userLoginWidget, &UserLoginWidget::updateName);
    m_currentUserConnects << connect(user.get(), &User::expiredInfoChanged, m_userLoginWidget, &UserLoginWidget::updateExpiredStatus);
    m_currentUserConnects << connect(user.get(), &User::keyboardLayoutListChanged, m_userLoginWidget, &UserLoginWidget::updateKeyboardList, Qt::UniqueConnection);
    m_currentUserConnects << connect(user.get(), &User::keyboardLayoutChanged, m_userLoginWidget, &UserLoginWidget::updateKeyboardInfo, Qt::UniqueConnection);
    m_currentUserConnects << connect(user.get(), &User::noPasswordLoginChanged, this, &UserLoginInfo::updateLoginContent); // TODO

    //需要清除上一个用户的验证状态数据
    if (m_user != nullptr && m_user != user) {
        m_userLoginWidget->clearAuthStatus();
    }
    m_user = user;

    m_userLoginWidget->updateName(user->displayName());
    m_userLoginWidget->updateAvatar(user->avatar());
    m_userLoginWidget->updateAuthType(m_model->currentType());
    /* 初始化锁定信息 */
    m_userLoginWidget->updateLimitsInfo(user->limitsInfo());
    /* 初始化验证状态*/
    m_userLoginWidget->updateAuthStatus();

    // m_userLoginWidget->updateIsLockNoPassword(m_model->isLockNoPassword());
    // m_userLoginWidget->disablePassword(user.get()->isLock(), user->lockTime());
    m_userLoginWidget->updateKeyboardList(user->keyboardLayoutList());
    m_userLoginWidget->updateKeyboardInfo(user->keyboardLayout());

    updateLoginContent();
}

void UserLoginInfo::initConnect()
{
    connect(m_userLoginWidget, &UserLoginWidget::requestAuthUser, this, [ = ](const QString & account, const QString & password) {
        // if (!m_userLoginWidget->inputInfoCheck(m_model->isServerModel())) return;

        //当前锁定不需要密码和当前用户不需要密码登录则直接进入系统
        if (m_model->isLockNoPassword() && m_model->currentUser()->isNoPasswordLogin()) {
            emit m_model->authFinished(true);
            return;
        }

        if (m_model->isServerModel() && m_model->currentUser()->type() == User::ADDomain) {
            auto user = dynamic_cast<NativeUser *>(m_model->findUserByName(account).get());
            auto current_user = m_model->currentUser();

            // static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserName(account);
            // static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserInter(nullptr);
            if (user != nullptr) {
                // static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserInter(user->userInter());
            }
        }
        // emit requestAuthUser(password);
    });
    // connect(m_model, &SessionBaseModel::authFaildMessage, m_userLoginWidget, &UserLoginWidget::setFaildMessage);
    connect(m_model, &SessionBaseModel::authFaildTipsMessage, m_userLoginWidget, &UserLoginWidget::setFaildTipMessage);
    connect(m_userLoginWidget, &UserLoginWidget::requestUserKBLayoutChanged, this, [=] (const QString &value) {
        emit requestSetLayout(m_user, value);
    });
    connect(m_userLoginWidget, &UserLoginWidget::unlockActionFinish, this, [&] {
        if (!m_userLoginWidget.isNull()) {
            //由于添加锁跳动会冲掉"验证完成"。这里只能临时关闭清理输入框
            // m_userLoginWidget->resetAllState();
        }
        emit unlockActionFinish();
    });

    //UserFrameList
    connect(m_userFrameList, &UserFrameList::clicked, this, &UserLoginInfo::hideUserFrameList);
    connect(m_userFrameList, &UserFrameList::requestSwitchUser, this, &UserLoginInfo::receiveSwitchUser);
    connect(m_model, &SessionBaseModel::abortConfirmChanged, this, &UserLoginInfo::abortConfirm);
    connect(m_userLoginWidget, &UserLoginWidget::clicked, this, [=] {
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode)
            m_model->setAbortConfirm(false);
    });

    //连接系统待机信号
    // connect(m_model, &SessionBaseModel::prepareForSleep, m_userLoginWidget, &UserLoginWidget::prepareForSleep, Qt::QueuedConnection);

    connect(m_model, &SessionBaseModel::authTypeChanged, m_userLoginWidget, &UserLoginWidget::updateWidgetShowType);
    connect(m_model, &SessionBaseModel::authStatusChanged, m_userLoginWidget, &UserLoginWidget::updateAuthResult);
    // connect(m_userLoginWidget, &UserLoginWidget::authFininshed, m_model, &SessionBaseModel::authFinished);
    connect(m_userLoginWidget, &UserLoginWidget::authFininshed, m_model, &SessionBaseModel::setVisible);
    connect(m_userLoginWidget, &UserLoginWidget::requestStartAuthentication, this, &UserLoginInfo::requestStartAuthentication);
    connect(m_userLoginWidget, &UserLoginWidget::sendTokenToAuth, this, &UserLoginInfo::sendTokenToAuth);
    connect(m_userLoginWidget, &UserLoginWidget::requestCheckAccount, this, &UserLoginInfo::requestCheckAccount);

    /* m_mode */
    connect(m_model->currentUser().get(), &User::limitsInfoChanged, m_userLoginWidget, &UserLoginWidget::updateLimitsInfo);
    connect(m_model, &SessionBaseModel::currentUserChanged, this, [=] (std::shared_ptr<User> user){
       connect(user.get(), &User::limitsInfoChanged, m_userLoginWidget, &UserLoginWidget::updateLimitsInfo);
    });
}

void UserLoginInfo::updateLocale()
{
    m_userLoginWidget->updateAccoutLocale();
}

void UserLoginInfo::abortConfirm(bool abort)
{
    if (!abort) {
        m_model->setPowerAction(SessionBaseModel::PowerAction::None);
    }

    m_shutdownAbort = abort;
    m_userLoginWidget->ShutdownPrompt(m_model->powerAction());
}

void UserLoginInfo::beforeUnlockAction(bool is_finish)
{
    if (is_finish) {
        m_userLoginWidget->unlockSuccessAni();
    } else {
        m_userLoginWidget->unlockFailedAni();
    }
}

UserLoginWidget *UserLoginInfo::getUserLoginWidget()
{
    return m_userLoginWidget;
}

UserFrameList *UserLoginInfo::getUserFrameList()
{
    return m_userFrameList;
}

void UserLoginInfo::hideKBLayout()
{
    // m_userLoginWidget->hideKBLayout();
}

void UserLoginInfo::userLockChanged(bool disable)
{
     // m_userLoginWidget->disablePassword(disable, m_user->lockTime());
}

void UserLoginInfo::receiveSwitchUser(std::shared_ptr<User> user)
{
    if (m_user != user) {
        // m_userLoginWidget->clearPassWord();

        abortConfirm(false);
    } else {
        emit switchToCurrentUser();
    }

    emit requestSwitchUser(user);
}

void UserLoginInfo::updateLoginContent()
{
    //在INT_MAX的切换用户时输入用户名，其他用户直接显示用户名
    if (m_model->currentUser()->uid() == INT_MAX) {
        // m_userLoginWidget->setWidgetShowType(UserLoginWidget::IDAndPasswordType);
    } else {
        if (m_user->isNoPasswordLogin()) {
            // m_userLoginWidget->setWidgetShowType(UserLoginWidget::NoPasswordType);
        } else {
            // m_userLoginWidget->setWidgetShowType(UserLoginWidget::NormalType);
        }
    }
}
