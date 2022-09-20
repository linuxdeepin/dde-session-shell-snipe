// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINCONTENT_H
#define LOGINCONTENT_H

#include "lockcontent.h"

class SessionWidget;
class LoginTipsWindow;

class LoginContent : public LockContent
{
    Q_OBJECT
public:
    explicit LoginContent(SessionBaseModel *const model, QWidget *parent = nullptr);

    void onCurrentUserChanged(std::shared_ptr<User> user) override;
    void onStatusChanged(SessionBaseModel::ModeStatus status) override;
    void pushSessionFrame();
    void pushTipsFrame();
    void popTipsFrame();

private:
    SessionWidget *m_sessionFrame;
    LoginTipsWindow *m_loginTipsWindow;
    bool m_showTipsWidget = true;
};

#endif // LOGINCONTENT_H
