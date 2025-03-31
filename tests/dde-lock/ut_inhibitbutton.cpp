// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inhibitbutton.h"

#include <QTest>

#include <gtest/gtest.h>

class UT_InhibitButton : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    InhibitButton *m_inhibitButton;
};

void UT_InhibitButton::SetUp()
{
    m_inhibitButton = new InhibitButton(nullptr);
}

void UT_InhibitButton::TearDown()
{
    delete  m_inhibitButton;
}

TEST_F(UT_InhibitButton, BasicTest) {
    QString test = "test";
    m_inhibitButton->setText(test);
    ASSERT_EQ(test, m_inhibitButton->text());

    QIcon icon = QIcon::fromTheme(":/img/inhibitview/shutdown.svg");
    m_inhibitButton->setNormalPixmap(icon.pixmap(30, 30));
    ASSERT_FALSE(m_inhibitButton->normalPixmap().isNull());
    m_inhibitButton->setHoverPixmap(icon.pixmap(30, 30));
    ASSERT_FALSE(m_inhibitButton->hoverPixmap().isNull());
}
