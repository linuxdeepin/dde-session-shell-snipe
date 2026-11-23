// SPDX-FileCopyrightText: 2018-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "dbusconstant.h"
#include <QSignalSpy>

#include "userinfo.h"

class UT_Interface : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
};

void UT_Interface::SetUp()
{

}

void UT_Interface::TearDown()
{

}

TEST_F(UT_Interface, user)
{
    std::shared_ptr<NativeUser> nativeUser(new NativeUser(QString(DSS_DBUS::accountsUserPath).arg(QString::number(getuid()))));
    // ASSERT_TRUE(nativeUser->userInter());
    // EXPECT_TRUE(nativeUser->userInter()->greeterBackground().isEmpty());
    // EXPECT_TRUE(nativeUser->userInter()->uid().isEmpty());
    // EXPECT_TRUE(nativeUser->userInter()->userName().isEmpty());
    // EXPECT_TRUE(nativeUser->userInter()->homeDir().isEmpty());
    // EXPECT_FALSE(nativeUser->userInter()->desktopBackgrounds().size() > 0);
    // EXPECT_TRUE(nativeUser->userInter()->isValid());
    // EXPECT_FALSE(nativeUser->userInter()->use24HourFormat());
    nativeUser->isUserValid();
    nativeUser->type();
    nativeUser->path();
    nativeUser->setKeyboardLayout("");
    nativeUser->updateAvatar("");
    nativeUser->updateAutomaticLogin(false);
    QStringList backgrounds;
    backgrounds << "";
    nativeUser->updateDesktopBackgrounds(backgrounds);
    nativeUser->updateFullName("");
    nativeUser->updateGreeterBackground("");
    nativeUser->updateKeyboardLayout("");
    QStringList keyboardLayout;
    keyboardLayout << "";
    nativeUser->updateKeyboardLayoutList(keyboardLayout);
    nativeUser->updateLocale("");
    nativeUser->updateName("");
    nativeUser->updateNoPasswordLogin(false);
    nativeUser->updatePasswordState("P");
    nativeUser->updateShortDateFormat(0);
    nativeUser->updateShortTimeFormat(0);
    nativeUser->updateWeekdayFormat(0);
    nativeUser->updateUid("");
    nativeUser->updateUse24HourFormat(true);

    std::shared_ptr<ADDomainUser> addomainUser(new ADDomainUser(getuid()));
    QString name = nativeUser->name();
    // nativeUser->userInter()->UserNameChanged(name.append("marks"));
    EXPECT_EQ(nativeUser->displayName(), name);
    addomainUser->setName(name);

    QString displayname = nativeUser->displayName();
    addomainUser->displayNameChanged(displayname.append("aaaaaa"));
    EXPECT_EQ(nativeUser->name(), name);
    addomainUser->setFullName(displayname);
}

TEST_F(UT_Interface, remoteLoginState)
{
    User user;
    QSignalSpy spy(&user, &User::remoteLoginStateChanged);

    EXPECT_FALSE(user.isRemoteLogin());
    user.updateRemoteLoginState(true);
    EXPECT_TRUE(user.isRemoteLogin());
    EXPECT_EQ(spy.count(), 1);

    user.updateRemoteLoginState(true);
    EXPECT_EQ(spy.count(), 1);

    User copy(user);
    EXPECT_TRUE(copy.isRemoteLogin());
}
