/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             Hualet <mr.asianwang@gmail.com>
 *             kirigaya <kirigaya@mkacg.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             Hualet <mr.asianwang@gmail.com>
 *             kirigaya <kirigaya@mkacg.com>
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

#include "public_func.h"
#include "constants.h"

#include <QFile>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDebug>
#include <QGSettings>
#include <QStandardPaths>
#include <QProcess>
#include <QDateTime>
#include <QDir>

#include <stdio.h>
#include <time.h>
#include <execinfo.h>
#include <sys/stat.h>
#include <signal.h>

using namespace std;

QPixmap loadPixmap(const QString &file, const QSize& size)
{

    if(!QFile::exists(file)){
        return QPixmap(DDESESSIONCC::LAYOUTBUTTON_HEIGHT,DDESESSIONCC::LAYOUTBUTTON_HEIGHT);
    }

    qreal ratio = 1.0;
    qreal devicePixel = qApp->devicePixelRatio();

    QPixmap pixmap;

    if (!qFuzzyCompare(ratio, devicePixel) || size.isValid()) {
        QImageReader reader;
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

/**
 * @brief 获取图像共享内存
 *
 * @param uid 当前用户ID
 * @param purpose 图像用途，1是锁屏、关机、登录，2是启动器，3-19是工作区
 * @return QString Qt的共享内存key
 */
QString readSharedImage(uid_t uid, int purpose)
{
    QDBusMessage msg = QDBusMessage::createMethodCall("com.deepin.dde.preload", "/com/deepin/dde/preload", "com.deepin.dde.preload", "requestSource");
    QList<QVariant> args;
    args.append(int(uid));
    args.append(purpose);
    msg.setArguments(args);
    QString shareKey;
    QDBusMessage ret = QDBusConnection::sessionBus().call(msg);
    if (ret.type() == QDBusMessage::ErrorMessage) {
        qDebug() << "readSharedImage fail. user: " << uid << ", purpose: " << purpose << ", detail: " << ret;
    } else {
        QDBusReply<QString> reply(ret);
        shareKey = reply.value();
    }
#ifdef QT_DEBUG
    qInfo() << __FILE__ << ", " << Q_FUNC_INFO << " user: " << uid << ", purpose: " << purpose << " share memory key: " << shareKey;
#endif
    return shareKey;
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
    const char* controlPath = "/com/deepin/dde/auth/control/";
    if (QGSettings::isSchemaInstalled (controlId)) {
        QGSettings controlObj (controlId, controlPath);
        bool bUseDeepinAuth =  controlObj.get ("use-deepin-auth").toBool();
    #ifdef QT_DEBUG
        qDebug() << "----------use deepin auth: " << bUseDeepinAuth;
    #endif
        return bUseDeepinAuth;
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
