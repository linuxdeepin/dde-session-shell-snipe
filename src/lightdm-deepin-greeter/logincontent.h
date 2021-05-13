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

#ifndef LOGINCONTENT_H
#define LOGINCONTENT_H

#include "src/session-widgets/lockcontent.h"
#include "src/widgets/wirelesswidget.h"

class SessionWidget;
class LoginContent : public LockContent
{
    Q_OBJECT
public:
    explicit LoginContent(SessionBaseModel *const model, QWidget *parent = nullptr);

    void onCurrentUserChanged(std::shared_ptr<User> user) override;
    void onStatusChanged(SessionBaseModel::ModeStatus status) override;
    void pushSessionFrame();
    void pushWirelessFrame();
    void updateWirelessDisplay();
    void onRequestWirelessPage();
    void updateWirelessListPosition();

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

private:
    SessionWidget *m_sessionFrame;
    WirelessWidget *m_wirelessWigdet;
    QWidget *m_wirelessConcealWidget;
};

#endif // LOGINCONTENT_H
