// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loginwindow.h"
#include "sessionbasemodel.h"
#include "userinfo.h"

#include <QResizeEvent>

#include <gtest/gtest.h>

class UT_LoginWindow : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    SessionBaseModel *m_sessionBaseModel;
    std::shared_ptr<User> m_user;
    LoginWindow *m_loginwindow;
};

void UT_LoginWindow::SetUp()
{
    m_sessionBaseModel = new SessionBaseModel();
    m_user = std::make_shared<User>();
    m_sessionBaseModel->updateCurrentUser(m_user);
    m_loginwindow = new LoginWindow(m_sessionBaseModel);
}

void UT_LoginWindow::TearDown()
{
    delete m_sessionBaseModel;
    delete m_loginwindow;
}

TEST_F(UT_LoginWindow, Validity)
{
    ASSERT_NE(m_loginwindow, nullptr);
}

TEST_F(UT_LoginWindow, Visibility)
{
    m_loginwindow->show();
    m_loginwindow->hide();
}
