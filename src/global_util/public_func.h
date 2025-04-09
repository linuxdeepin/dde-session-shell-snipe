// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PUBLIC_FUNC_H
#define PUBLIC_FUNC_H

#include "constants.h"

#include <QPixmap>
#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QSettings>
#include <QString>

class QJsonDocument;

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

int setRootWindowCursor();
void setPointer();

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

/**
 * @brief 设置app类型，让程序知道应该获取哪个配置文件
 */
void setAppType(int type);

/**
 * @brief 获取默认的配置文件名称
 */
QString getDefaultConfigFileName();

void loadTranslation(const QString &locale);
QString findSymLinTarget(const QString &symLink);

QString toJson(const QJsonObject &jsonObj);

bool checkVersion(const QString &target, const QString &base);

/**
 * @brief 配置 qwebengine 相关环境
 */
void configWebEngine();

#ifdef ENABLE_DSS_SNIPE
bool isSleepLock();
#endif

#endif // PUBLIC_FUNC_H
