// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionconflict.h"

#include <gtest/gtest.h>

TEST(SessionConflict, InvalidInputHasNoConflict)
{
    EXPECT_FALSE(SessionConflict::hasActiveSessionConflict(nullptr, QString()));
    EXPECT_FALSE(SessionConflict::hasActiveSessionConflict(nullptr, QStringLiteral("...")));
}

TEST(SessionConflict, InvalidInputNotRemote)
{
    EXPECT_FALSE(SessionConflict::isCurrentSessionRemote(nullptr));
}

TEST(SessionConflict, InvalidInputNoRemoteUsers)
{
    const QString alice = QStringLiteral("alice");
    EXPECT_EQ(SessionConflict::countActiveRemoteUsers(nullptr, QString()), 0);
    EXPECT_EQ(SessionConflict::countActiveRemoteUsers(nullptr, alice), 0);
}
