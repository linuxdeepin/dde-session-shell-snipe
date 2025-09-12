// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_COMPAT_HELPER_H
#define QT_COMPAT_HELPER_H

#include <QDateTime>

class QString;

/**
 * @brief Qt版本兼容性帮助类
 *
 * 用于统一处理Qt5和Qt6之间的API差异，避免在代码中到处使用版本检查宏。
 * 主要解决QDateTime相关方法在不同Qt版本中的差异。
 */
class QtCompatHelper
{
public:
    /**
     * @brief 获取QDateTime对象对应的秒级时间戳
     *
     * 在Qt5中使用toTime_t()方法，在Qt6中使用toSecsSinceEpoch()方法
     *
     * @param dateTime QDateTime对象
     * @return uint 秒级时间戳
     */
    static uint toSecsSinceEpoch(const QDateTime &dateTime);
};

#endif // QT_COMPAT_HELPER_H
