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
#include "userexpiredwidget.h"
#include "sessionbasemodel.h"
#include "userframelist.h"
#include "src/global_util/constants.h"
#include <QKeyEvent>

UserLoginInfo::UserLoginInfo(SessionBaseModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_userLoginWidget(new UserLoginWidget)
    , m_userExpiredWidget(new UserExpiredWidget)
    , m_userFrameList(new UserFrameList)
{
    m_userLoginWidget->setWidgetWidth(DDESESSIONCC::PASSWDLINEEIDT_WIDTH);
    m_userExpiredWidget->setFixedWidth(DDESESSIONCC::PASSWDLINEEIDT_WIDTH);
    m_userFrameList->setModel(model);
    initConnect();
}

void UserLoginInfo::setUser(std::shared_ptr<User> user)
{
    for (auto connect : m_currentUserConnects) {
        m_user->disconnect(connect);
    }

    m_currentUserConnects.clear();
    m_currentUserConnects << connect(user.get(), &User::lockChanged, this, &UserLoginInfo::userLockChanged);
    m_currentUserConnects << connect(user.get(), &User::avatarChanged, m_userLoginWidget, &UserLoginWidget::setAvatar);
    m_currentUserConnects << connect(user.get(), &User::displayNameChanged, m_userLoginWidget, &UserLoginWidget::setName);
    m_currentUserConnects << connect(user.get(), &User::kbLayoutListChanged, m_userLoginWidget, &UserLoginWidget::updateKBLayout, Qt::UniqueConnection);
    m_currentUserConnects << connect(user.get(), &User::currentKBLayoutChanged, m_userLoginWidget, &UserLoginWidget::setDefaultKBLayout, Qt::UniqueConnection);
    m_currentUserConnects << connect(user.get(), &User::noPasswdLoginChanged, this, &UserLoginInfo::updateLoginContent);

    m_user = user;

    m_userLoginWidget->setIsServerMode(m_model->isServerModel());
    m_userLoginWidget->setName(user->displayName());
    m_userLoginWidget->setAvatar(user->avatarPath());
    m_userLoginWidget->setUserAvatarSize(UserLoginWidget::AvatarLargeSize);
    m_userLoginWidget->updateAuthType(m_model->currentType());
    m_userLoginWidget->updateIsLockNoPassword(m_model->isLockNoPassword());
    m_userLoginWidget->disablePassword(user.get()->isLock(), user->lockTime());
    m_userLoginWidget->setKBLayoutList(user->kbLayoutList());
    m_userLoginWidget->setDefaultKBLayout(user->currentKBLayout());

    m_userExpiredWidget->setDisplayName(user->displayName());
    m_userExpiredWidget->setUserName(user->name());
    m_userExpiredWidget->updateAuthType(m_model->currentType());

    updateLoginContent();
}

void UserLoginInfo::initConnect()
{
    //UserLoginWidget
    connect(m_userLoginWidget, &UserLoginWidget::requestAuthUser, this, [ = ](const QString & account, const QString & password) {
        if (!m_userLoginWidget->inputInfoCheck(m_model->isServerModel())) return;

        //当前锁定不需要密码和当前用户不需要密码登录则直接进入系统
        if(m_model->isLockNoPassword() && m_model->currentUser()->isNoPasswdGrp()) {
            emit m_model->authFinished(true);
            return;
        }

        if (m_model->isServerModel() && m_model->currentType() == SessionBaseModel::LightdmType) {
            auto user = dynamic_cast<NativeUser *>(m_model->findUserByName(account).get());
            auto current_user = m_model->currentUser();

            static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserName(account);
            static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserInter(nullptr);
            if (user != nullptr) {
                static_cast<ADDomainUser *>(m_model->currentUser().get())->setUserInter(user->getUserInter());
                m_userExpiredWidget->setDisplayName(user->displayName());
                m_userExpiredWidget->setUserName(account);
            }
        }

        if (m_model->currentUser()->isPasswordExpired()) {
            m_userExpiredWidget->setPassword(password);
        }
        emit requestAuthUser(password);
    });
    connect(m_model, &SessionBaseModel::authFaildMessage, m_userLoginWidget, &UserLoginWidget::setFaildMessage);
    connect(m_model, &SessionBaseModel::authFaildTipsMessage, m_userLoginWidget, &UserLoginWidget::setFaildTipMessage);
    connect(m_userLoginWidget, &UserLoginWidget::requestUserKBLayoutChanged, this, [ = ](const QString & value) {
        emit requestSetLayout(m_user, value);
    });
    connect(m_userLoginWidget, &UserLoginWidget::unlockActionFinish, this, [&]{
        if (!m_userLoginWidget.isNull()) {
            //由于添加锁跳动会冲掉"验证完成"。这里只能临时关闭清理输入框
            m_userLoginWidget->resetAllState();
        }
        emit unlockActionFinish();
    });
    connect(m_userExpiredWidget, &UserExpiredWidget::changePasswordFinished, this, &UserLoginInfo::changePasswordFinished);

    //UserFrameList
    connect(m_userFrameList, &UserFrameList::clicked, this, &UserLoginInfo::hideUserFrameList);
    connect(m_userFrameList, &UserFrameList::requestSwitchUser, this, &UserLoginInfo::receiveSwitchUser);
    connect(m_model, &SessionBaseModel::abortConfirmChanged, this, &UserLoginInfo::onAbortConfirmChanged);
    connect(m_userLoginWidget, &UserLoginWidget::clicked, this, [=] {
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode)
            m_model->setAbortConfirm(false);
    });
}

void UserLoginInfo::onAbortConfirmChanged(bool abort)
{
    if (!abort) {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
    }

    abortConfirm(abort);
}

void UserLoginInfo::abortConfirm(bool abort)
{
    if (!abort) {
        m_model->setPowerAction(SessionBaseModel::PowerAction::RequireNormal);
    }

    m_shutdownAbort = abort;
    m_userLoginWidget->ShutdownPrompt(m_model->powerAction());
}

void UserLoginInfo::beforeUnlockAction(bool is_finish)
{
    if(is_finish){
        m_userLoginWidget->unlockSuccessAni();
    }else {
        m_userLoginWidget->unlockFailedAni();
    }
}

UserLoginWidget *UserLoginInfo::getUserLoginWidget()
{
    m_userExpiredWidget->resetAllState();
    return m_userLoginWidget;
}

UserFrameList *UserLoginInfo::getUserFrameList()
{
    return m_userFrameList;
}

UserExpiredWidget *UserLoginInfo::getUserExpiredWidget()
{
    m_userLoginWidget->resetAllState();
    return m_userExpiredWidget;
}

void UserLoginInfo::hideKBLayout()
{
    m_userLoginWidget->hideKBLayout();
}

void UserLoginInfo::userLockChanged(bool disable)
{
    m_userLoginWidget->disablePassword(disable, m_user->lockTime());
    emit m_model->lockChanged(disable);
}

void UserLoginInfo::receiveSwitchUser(std::shared_ptr<User> user)
{
    if (m_user != user) {
        m_userLoginWidget->clearPassWord();

        abortConfirm(false);
    } else {
        emit switchToCurrentUser();
    }

    emit requestSwitchUser(user);
}

void UserLoginInfo::updateLoginContent()
{
    if (m_model->currentUser()->isDoMainUser()) {
        m_userLoginWidget->setWidgetShowType(UserLoginWidget::IDAndPasswordType);
    } else {
        if (m_user->isNoPasswdGrp()) {
            m_userLoginWidget->setWidgetShowType(UserLoginWidget::NoPasswordType);
        } else {
            m_userLoginWidget->setWidgetShowType(UserLoginWidget::NormalType);
        }
    }
}
