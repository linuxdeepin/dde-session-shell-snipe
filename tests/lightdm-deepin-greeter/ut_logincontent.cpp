// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logincontent.h"
#include "sessionbasemodel.h"

#include <gtest/gtest.h>

class UT_LoginContent : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    SessionBaseModel *m_model;
    LoginContent *m_content;
};

void UT_LoginContent::SetUp()
{
    m_model = new SessionBaseModel();
    std::shared_ptr<User> user_ptr(new User);
    m_model->updateCurrentUser(user_ptr);
    LoginContent::instance()->init(m_model);
}

void UT_LoginContent::TearDown()
{

}

TEST_F(UT_LoginContent, BasicTest)
{
    LoginContent::instance()->onCurrentUserChanged(m_model->currentUser());
    LoginContent::instance()->pushSessionFrame();
    LoginContent::instance()->pushTipsFrame();
    LoginContent::instance()->popTipsFrame();
}

TEST_F(UT_LoginContent, ModeTest)
{
    LoginContent::instance()->onStatusChanged(SessionBaseModel::NoStatus);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::PowerMode);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::ConfirmPasswordMode);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::UserMode);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::SessionMode);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::PowerMode);
    LoginContent::instance()->onStatusChanged(SessionBaseModel::ShutDownMode);
    LoginContent::instance()->restoreMode();
}
