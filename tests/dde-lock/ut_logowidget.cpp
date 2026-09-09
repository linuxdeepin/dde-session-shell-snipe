// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logowidget.h"
#include "constants.h"

#include <gtest/gtest.h>
#include <DSysInfo>
#include <QLabel>

DCORE_USE_NAMESPACE
using namespace DDESESSIONCC;

class UT_LogWidget : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    LogoWidget *logoWidget;
};

void UT_LogWidget::SetUp()
{
    logoWidget = new LogoWidget();
}

void UT_LogWidget::TearDown()
{
    delete logoWidget;
}

TEST_F(UT_LogWidget, init)
{
    logoWidget->updateLocale("en_US.UTF-8");
}

TEST_F(UT_LogWidget, versionText)
{
    // 教育版不显示系统版本信息
    if (DSysInfo::UosEdition::UosEducation == DSysInfo::uosEditionType()) {
        GTEST_SKIP() << "Education edition does not show system version";
    }

    QLabel *versionLabel = logoWidget->findChild<QLabel *>("LogoVersion");
    ASSERT_NE(versionLabel, nullptr);

    const QString defaultText = versionLabel->text();
    EXPECT_FALSE(defaultText.isEmpty());

    // 配置项不为空时，显示配置的文本内容
    logoWidget->updateVersionText("25 专效版");
    EXPECT_EQ(versionLabel->text(), QString("25 专效版"));

    // 配置项为空时，回退到系统默认的版本号+系统类型
    logoWidget->updateVersionText("");
    EXPECT_EQ(versionLabel->text(), defaultText);

    // dconfig 变更回调
    LogoWidget::onDConfigPropertyChanged(SYSTEM_VERSION_TEXT, "26 专效版", logoWidget);
    EXPECT_EQ(versionLabel->text(), QString("26 专效版"));

    LogoWidget::onDConfigPropertyChanged(SYSTEM_VERSION_TEXT, "", logoWidget);
    EXPECT_EQ(versionLabel->text(), defaultText);
}
