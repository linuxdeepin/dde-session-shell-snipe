// SPDX-FileCopyrightText: 2011 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINCONTENT_H
#define LOGINCONTENT_H

#include "lockcontent.h"
#include "userswiththesamename.h"

class QProcess;
class SessionWidget;
class LoginTipsWindow;

class LoginContent : public LockContent
{
    Q_OBJECT

public:
    explicit LoginContent(QWidget *parent = nullptr);
    static LoginContent *instance();
    void init(SessionBaseModel *model);
    void onCurrentUserChanged(std::shared_ptr<User> user) override;
    void onStatusChanged(SessionBaseModel::ModeStatus status) override;
    void reactivateAccountEntry();
    void setVisible(bool visible) override;
    bool usesAccountEntry() const;
    void setGreeterActive(bool active);
    bool accountEntryBlocksLogin() const { return m_taskProcess || m_startingAccountEntry || !m_greeterActive; }
    void pushSessionFrame();
    void pushTipsFrame();
    void popTipsFrame();
    void showUsersWithTheSameName(const QString &nativeUserName, const QString &doMainAccountDetail);

signals:
    void accountEntryVisibilityChanged();

protected:
    void onSwitchUserRequested() override;

private:
    bool beginAccountEntry();
    void enterAccountInput();
    void startAccountEntryTask(const QString &program, bool selecting);

    QProcess *m_taskProcess = nullptr;
    bool m_greeterActive = true;
    bool m_accountEntryPending = false;
    bool m_startingAccountEntry = false;

    SessionWidget *m_sessionFrame;
    LoginTipsWindow *m_loginTipsWindow;
    bool m_showTipsWidget = true;
    UsersWithTheSameName *m_usersWithTheSameName;
};

#endif // LOGINCONTENT_H
