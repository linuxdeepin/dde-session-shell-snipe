// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt-compat-helper.h"

#include <QDateTime>
#include <QString>

uint QtCompatHelper::toSecsSinceEpoch(const QDateTime &dateTime)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return static_cast<uint>(dateTime.toSecsSinceEpoch());
#else
    return static_cast<uint>(dateTime.toTime_t());
#endif
}
