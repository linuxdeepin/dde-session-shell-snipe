// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGIN_PLUGIN_UTIL_H
#define LOGIN_PLUGIN_UTIL_H

#include "login_plugin.h"

class LPUtil
{
public:
    using Type = LoginPlugin::CustomLoginType;

    static int loginType();
    static int updateLoginType();
    static int sessionTimeout();
    static bool hasSecondLevel(const QString &user);
    static LoginPlugin* getLevel2LoginPlugin();
};


#endif //LOGIN_PLUGIN_UTIL_H
