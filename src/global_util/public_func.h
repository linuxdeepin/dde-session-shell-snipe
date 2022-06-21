/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PUBLIC_FUNC_H
#define PUBLIC_FUNC_H

#include "constants.h"

#include <QPixmap>
#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QSettings>
#include <QString>

#define ACCOUNTS_DBUS_PREFIX "/com/deepin/daemon/Accounts/User"

static const int APP_TYPE_LOCK = 0;
static const int APP_TYPE_LOGIN = 1;

QPixmap loadPixmap(const QString &file, const QSize& size = QSize());
void loadPixmap(const QString &fileName, QPixmap &pixmap);
bool checkPictureCanRead(const QString &fileName);

template <typename T>
T findValueByQSettings(const QStringList &configFiles,
                       const QString &group,
                       const QString &key,
                       const QVariant &fallback)
{
    for (const QString &path : configFiles) {
        QSettings settings(path, QSettings::IniFormat);
        if (!group.isEmpty()) {
            settings.beginGroup(group);
        }

        const QVariant& v = settings.value(key);
        if (v.isValid()) {
            T t = v.value<T>();
            return t;
        }
    }

    return fallback.value<T>();
}

/**
 * @brief 是否使用深度认证，不使用域管认证。
 *
 * @return true 使用深度认证
 * @return false 使用域管认证
 */
bool isDeepinAuth();

/**
 * @brief 把字符串解析成时间，然后转换为Unix时间戳
 */
uint timeFromString(QString time);

QVariant getDConfigValue(const QString &configFileName, const QString &key, const QVariant &defaultValue);

void setDConfigValue(const QString &configFileName, const QString &key, const QVariant &value);

/**
 * @brief 设置app类型，让程序知道应该获取哪个配置文件
 */
void setAppType(int type);

/**
 * @brief 获取默认的配置文件名称
 */
QString getDefaultConfigFileName();

void loadTranslation(const QString &locale);

#endif // PUBLIC_FUNC_H
