// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardplantform_wayland.h"

#include <gtest/gtest.h>

#ifdef USE_DEEPIN_WAYLAND
class UT_KeyboardPlantformWayland : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    KeyboardPlatformWayland *m_keyboard;
};

void UT_KeyboardPlantformWayland::SetUp()
{
    m_keyboard = new KeyboardPlatformWayland;
}

void UT_KeyboardPlantformWayland::TearDown()
{
    delete m_keyboard;
}

TEST_F(UT_KeyboardPlantformWayland, basic)
{
    m_keyboard->isCapsLockOn();
    m_keyboard->isNumLockOn();
    m_keyboard->setNumLockStatus(false);
}
#endif
