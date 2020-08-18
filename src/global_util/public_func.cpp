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
#include <QFile>
#include "constants.h"
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDebug>
#include <QGSettings>

QPixmap loadPixmap(const QString &file)
{

    if(!QFile::exists(file)){
        return QPixmap(DDESESSIONCC::LAYOUTBUTTON_HEIGHT,DDESESSIONCC::LAYOUTBUTTON_HEIGHT);
    }

    qreal ratio = 1.0;
    qreal devicePixel = qApp->devicePixelRatio();

    QPixmap pixmap;

    if (!qFuzzyCompare(ratio, devicePixel)) {
        QImageReader reader;
        reader.setFileName(qt_findAtNxFile(file, devicePixel, &ratio));
        if (reader.canRead()) {
            reader.setScaledSize(reader.size() * (devicePixel / ratio));
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
 * @brief 判断某一个GSettings是否已安装且指定的键的值是否为true
 * 
 * @param id GSettings的ID
 * @param path GSettings的路径
 * @param keyName 键名
 * @return true true表示已配置为true
 * @return false false表示GSettings未安装或已安装但其值被设置为false
 */
bool isSettingConfigured(const QString& id, const QString& path, const QString keyName) 
{
    if (!QGSettings::isSchemaInstalled(id.toUtf8())) {
        return false;
    }
    QGSettings setting(id.toUtf8(), path.toUtf8());
    QVariant v = setting.get(keyName);
    if (!v.isValid()) {
        return false;
    }
    return v.toBool();
}
