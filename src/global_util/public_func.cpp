// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "public_func.h"

#include "constants.h"

#include <DConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGSettings>
#include <QProcess>
#include <QStandardPaths>
#include <QTranslator>
#include <QJsonDocument>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

using namespace std;

DCORE_USE_NAMESPACE

static int appType = APP_TYPE_LOCK;

QPixmap loadPixmap(const QString &file, const QSize& size)
{
    qreal ratio = 1.0;
    qreal devicePixel = qApp->devicePixelRatio();

    QPixmap pixmap;

    if (!qFuzzyCompare(ratio, devicePixel) || size.isValid()) {
        QImageReader reader;
        reader.setDecideFormatFromContent(true);
        reader.setFileName(qt_findAtNxFile(file, devicePixel, &ratio));
        if (reader.canRead()) {
            reader.setScaledSize((size.isNull() ? reader.size() : reader.size().scaled(size, Qt::KeepAspectRatio)) * (devicePixel / ratio));
            pixmap = QPixmap::fromImage(reader.read());
            pixmap.setDevicePixelRatio(devicePixel);
        }
    } else {
        pixmap.load(file);
    }

    return pixmap;
}

void loadPixmap(const QString &fileName, QPixmap &pixmap)
{
    if (!pixmap.load(fileName)) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            pixmap.loadFromData(file.readAll());
        }
    }
}

bool checkPictureCanRead(const QString &fileName)
{
    QImageReader reader;
    reader.setDecideFormatFromContent(true);
    reader.setFileName(fileName);
    return reader.canRead();
}

/**
 * @brief 是否使用域管认证。
 *
 * @return true 使用域管认证
 * @return false 使用系统认证
 */
bool isDeepinAuth()
{
    const char* controlId = "com.deepin.dde.auth.control";
    if (QGSettings::isSchemaInstalled(controlId)) {
        const char *controlPath = "/com/deepin/dde/auth/control/";
        QGSettings controlObj(controlId, controlPath);
        const QString &key = "useDeepinAuth";
        bool useDeepinAuth = controlObj.keys().contains(key) && controlObj.get(key).toBool();
    #ifdef QT_DEBUG
        qDebug() << "use deepin auth: " << useDeepinAuth;
    #endif
        return useDeepinAuth;
    }
    return true;
}

uint timeFromString(QString time)
{
    if (time.isEmpty()) {
        return QDateTime::currentDateTime().toTime_t();
    }
    return QDateTime::fromString(time, Qt::ISODateWithMs).toLocalTime().toTime_t();
}

/**
 * @brief getDConfigValue 根据传入的\a key获取配置项的值，获取失败返回默认值
 * @param key 配置项键值
 * @param defaultValue 默认返回值，为避免出现返回值错误导致程序异常的问题，此参数必填
 * @param configFileName 配置文件名称
 * @return 配置项的值
 */
QVariant getDConfigValue(const QString &configFileName, const QString &key, const QVariant &defaultValue)
{
    if (configFileName.isEmpty())
        return defaultValue;

    DConfig config(configFileName);
    if (!config.isValid() || !config.keyList().contains(key)) {
        qWarning() << "dconfig parse failed, name: " << config.name()
                   << "subpath: " << config.subpath()
                   << "\n use fallback value:" << defaultValue;
        return defaultValue;
    }

    return config.value(key);
}

/**
 * @brief setDConfigValue 根据传入的\a key设置对应的值
 * @param configFileName 配置文件名称
 * @param key 配置项键值
 * @param value 值
 * @return 配置项的值
 */
void setDConfigValue(const QString &configFileName, const QString &key, const QVariant &value)
{
    if (configFileName.isEmpty())
        return;

    DConfig config(configFileName);
    if (!config.isValid() || !config.keyList().contains(key)) {
        qWarning() << "dconfig parse failed, name: " << config.name()
                   << "subpath: " << config.subpath();
        return;
    }

    config.setValue(key, value);
}

void setAppType(int type)
{
    appType = type;
}

QString getDefaultConfigFileName()
{
    return APP_TYPE_LOCK == appType ? DDESESSIONCC::LOCK_DCONFIG_SOURCE : DDESESSIONCC::LOGIN_DCONFIG_SOURCE;
}

/**
 * @brief 加载翻译文件
 *
 * @param locale
 */
void loadTranslation(const QString &locale)
{
    static QTranslator translator;
    static QString localTmp;
    if (localTmp == locale) {
        return;
    }
    localTmp = locale;
    qApp->removeTranslator(&translator);
    translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + locale.split(".").first());
    qApp->installTranslator(&translator);
}

/**
 * @brief findSymLinTarget 查找软连接最终链接到的文件，如果不是软连接，直接返回原始路径
 * @param symLink 文件路径
 * @return 最终链接到的文件路径
 */
QString findSymLinTarget(const QString &symLink)
{
    QString file = symLink;
    QString tmpFile = symLink;
    while(!tmpFile.isEmpty()) {
        file = tmpFile;
        tmpFile = QFile::symLinkTarget(tmpFile);
    }

    return file;
}

/**
 * @brief json对象转为json字符串
 *
 * @param QJsonObject对象
 */
QString toJson(const QJsonObject &jsonObj)
{
    QJsonDocument doc;
    doc.setObject(jsonObj);
    return doc.toJson();
}

bool checkVersion(const QString &target, const QString &base)
{
    if (target == base) {
        return true;
    }
    const QStringList baseVersion = base.split(".");
    const QStringList targetVersion = target.split(".");

    const int baseVersionSize = baseVersion.size();
    const int targetVersionSize = targetVersion.size();
    const int size = baseVersionSize < targetVersionSize ? baseVersionSize : targetVersionSize;

    for (int i = 0; i < size; i++) {
        if (targetVersion[i] == baseVersion[i]) continue;
        return targetVersion[i].toInt() > baseVersion[i].toInt() ? true : false;
    }
    return true;
}
