/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     justforlxz <justforlxz@outlook.com>
 *
 * Maintainer: justforlxz <justforlxz@outlook.com>
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

#include "logincontent.h"
#include "src/widgets/sessionwidget.h"
#include "src/widgets/controlwidget.h"

LoginContent::LoginContent(SessionBaseModel *const model, QWidget *parent)
    : LockContent(model, parent)
{
    m_sessionFrame = new SessionWidget;
    m_sessionFrame->setModel(model);
    m_controlWidget->setSessionSwitchEnable(m_sessionFrame->sessionCount() > 1);

    connect(m_sessionFrame, &SessionWidget::hideFrame, this, &LockContent::restoreMode);
    connect(m_sessionFrame, &SessionWidget::sessionChanged, this, &LockContent::restoreMode);
    connect(m_controlWidget, &ControlWidget::requestSwitchSession, this, [ = ] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::SessionMode);
    });
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, m_controlWidget, &ControlWidget::chooseToSession);
    connect(m_model, &SessionBaseModel::onSessionKeyChanged, this, &LockContent::restoreMode);
}

void LoginContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (user.get() == nullptr) return;

    LockContent::onCurrentUserChanged(user);
    m_sessionFrame->switchToUser(user->name());
}

void LoginContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    switch (status) {
    case SessionBaseModel::ModeStatus::SessionMode:
        pushSessionFrame();
        break;
    default:
        LockContent::onStatusChanged(status);
        break;
    }
}

void LoginContent::pushSessionFrame()
{
    setCenterContent(m_sessionFrame);
    m_sessionFrame->show();
}
