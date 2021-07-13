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
#include "src/global_util/constants.h"
#include <QKeyEvent>
#include <unistd.h>
#include <pwd.h>

UserLoginInfo::UserLoginInfo(SessionBaseModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_userLoginWidget(new UserLoginWidget)
    , m_userFrameList(new UserFrameList)
{
    m_userLoginWidget->setWidgetWidth(DDESESSIONCC::PASSWDLINEEIDT_WIDTH);
    m_userFrameList->setModel(model);
    initConnect();
}

UserLoginInfo::~UserLoginInfo()
{
    m_userFrameList->deleteLater();
    m_userLoginWidget->deleteLater();
}

void UserLoginInfo::setUser(std::shared_ptr<User> user)
{
    for (auto connect : m_currentUserConnects) {
        m_user->disconnect(connect);
    }

    m_currentUserConnects.clear();
    m_currentUserConnects << connect(user.get(), &User::lockChanged, this, &UserLoginInfo::userLockChanged);
    m_currentUserConnects << connect(user.get(), &User::lockLimitFinished, m_model, &SessionBaseModel::lockLimitFinished);
    m_currentUserConnects << connect(user.get(), &User::lockLimitFinished, this, &UserLoginInfo::userLockChanged);
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
    m_userLoginWidget->disablePassword(user->isLock(), user->lockTime());
    user->kbLayoutList();
    user->currentKBLayout();

    updateLoginContent();
}

void UserLoginInfo::initConnect()
{
    //UserLoginWidget
    connect(m_userLoginWidget, &UserLoginWidget::requestCheckAccount, this, [ = ](const QString & account) {
        if (account.isEmpty()) return ;
        if (m_model->isServerModel() && m_model->currentUser()->isDoMainUser()) {
            auto user = dynamic_cast<NativeUser *>(m_model->findUserByName(account).get());
            auto current_user = dynamic_cast<ADDomainUser *>(m_model->currentUser().get());

            if (!current_user->name().isEmpty()) {
                //查找上一个用户并断开密码锁定相关信号
                auto tmpUser = m_model->findUserByName(current_user->name());
                if (tmpUser != nullptr) {
                    disconnect(tmpUser.get(), &User::lockChanged, current_user, &User::lockChanged);
                    disconnect(tmpUser.get(), &User::lockLimitFinished, current_user, &ADDomainUser::lockLimitFinished);
                }
            }

            current_user->setUserInter(nullptr);

            if (user != nullptr) {
                current_user->setUserName(account);
                current_user->setUserInter(user->getUserInter());

                //连接用户密码锁定相关信息到当前用户
                connect(user, &User::lockChanged, current_user, &ADDomainUser::lockChanged);
                connect(user, &User::lockLimitFinished, current_user, &ADDomainUser::lockLimitFinished);

                //更新锁定状态到当前登录界面
                m_userLoginWidget->disablePassword(user->isLock(), user->lockTime());
            } else {
                current_user->setUserName("");
                m_userLoginWidget->disablePassword(false, 0);
                m_userLoginWidget->setFaildTipMessage(tr("Wrong account"));
            }
        }
    });

    connect(m_userLoginWidget, &UserLoginWidget::requestAuthUser, this, [ = ](const QString & account, const QString & password) {
        if (!m_userLoginWidget->inputInfoCheck(m_model->isServerModel())) return;

        //当前锁定不需要密码和当前用户不需要密码登录则直接进入系统
        if (m_model->isLockNoPassword() && m_model->currentUser()->isNoPasswdGrp()) {
            emit m_model->authFinished(true);
            return;
        }

        //远程账号不能用Server模式处理，因为域账号也有后端dbus服务，ADDomainuser类也不适合远程账号
        //所以把ADDomainUser类做成SessionBaseModel非服务器器模式的切换远程账号的入口
        if (m_model->isServerModel() || m_model->currentUser()->isDoMainUser()) {
            auto user = dynamic_cast<NativeUser *>(m_model->findUserByName(account).get());
            auto current_user = dynamic_cast<ADDomainUser *>(m_model->currentUser().get());

            if (!current_user->name().isEmpty()) {
                //查找上一次准备登录的用户并断开密码锁定相关信号
                auto tmpUser = m_model->findUserByName(current_user->name());
                if (tmpUser != nullptr) {
                    disconnect(tmpUser.get(), &User::lockChanged, current_user, &User::lockChanged);
                    disconnect(tmpUser.get(), &User::lockLimitFinished, current_user, &ADDomainUser::lockLimitFinished);
                }
            }

            current_user->setUserInter(nullptr);

            if (user != nullptr) {
                current_user->setUserName(account);
                current_user->setUserInter(user->getUserInter());

                //连接用户密码锁定相关信息到当前登录界面
                connect(user, &User::lockChanged, current_user, &ADDomainUser::lockChanged);
                connect(user, &User::lockLimitFinished, current_user, &ADDomainUser::lockLimitFinished);

                //更新锁定状态到当前登录界面
                m_userLoginWidget->disablePassword(user->isLock(), user->lockTime());

                //如果用户密码被锁定，则不开启密码验证
                if (user->isLock()) return;
            } else {
                //用户不存在则不开启验证
                current_user->setUserName("");
                m_userLoginWidget->disablePassword(false, 0);
                m_userLoginWidget->setFaildTipMessage(tr("Wrong account"));
                return;
            }
        }
        emit requestAuthUser(password);
    });
    connect(m_model, &SessionBaseModel::authFaildMessage, m_userLoginWidget, &UserLoginWidget::setFaildMessage);
    connect(m_model, &SessionBaseModel::authFaildTipsMessage, m_userLoginWidget, &UserLoginWidget::setFaildTipMessage);
    connect(m_userLoginWidget, &UserLoginWidget::requestUserKBLayoutChanged, this, [ = ](const QString & value) {
        emit requestSetLayout(m_user, value);
    });
    connect(m_userLoginWidget, &UserLoginWidget::unlockActionFinish, this, [&] {
        if (!m_userLoginWidget.isNull())
        {
            //由于添加锁跳动会冲掉"验证完成"。这里只能临时关闭清理输入框
            m_userLoginWidget->resetAllState();
        }
        emit unlockActionFinish();
    });

    //UserFrameList
    connect(m_userFrameList, &UserFrameList::clicked, this, &UserLoginInfo::hideUserFrameList);
    connect(m_userFrameList, &UserFrameList::requestSwitchUser, this, &UserLoginInfo::receiveSwitchUser);
    connect(m_model, &SessionBaseModel::abortConfirmChanged, this, &UserLoginInfo::onAbortConfirmChanged);
    connect(m_userLoginWidget, &UserLoginWidget::clicked, this, [ = ] {
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
    m_userLoginWidget->hideKBLayout();
}

void UserLoginInfo::userLockChanged(bool disable)
{
    // //在登录服务器时，当前登录用户永远是...,需要根据用户查找真实用户的锁定时间信息
    auto tmpUser = m_user;
    if (tmpUser != nullptr && tmpUser->uid() == INT_MAX) {
        tmpUser = m_model->findUserByName(tmpUser->name());
    }
    if (tmpUser == nullptr) return;

    m_userLoginWidget->disablePassword(disable, tmpUser->lockTime());
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
    //在INT_MAX的切换用户时输入用户名，其他用户直接显示用户名
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
