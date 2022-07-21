/*
 * Copyright (C) 2022 ~ 2022 Deepin Technology Co., Ltd.
 *
 * Author:     chenbin <chenbina@uniontech.com>
 *
 * Maintainer: chenbin <chenbina@uniontech.com>
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

#ifndef GREETER_DISPLAY_WAYLAND_H
#define GREETER_DISPLAY_WAYLAND_H

#include <QObject>
#include <QJsonObject>

class QThread;
class QTimer;

namespace KWayland
{
namespace Client
{
    class Registry;
    class ConnectionThread;
    class EventQueue;
    class OutputDevice;
    class OutputConfiguration;
    class OutputManagement;
}
}

using namespace KWayland::Client;

struct MonitorConfig
{
    QString uuid;
    QString name;
    bool enabled;
    int x;
    int y;
    int width;
    int height;
    double refresh_rate;
    int transform;
    double scale;
    double brightness;
    bool primary;
    bool hasCof;
    OutputDevice *dev;
};

class GreeterDisplayWayland : public QObject
{
    Q_OBJECT

public:
    enum DisplayMode {
        Custom_Mode,
        Mirror_Mode,
        Extend_Mode,
        Single_Mode
    };
    GreeterDisplayWayland(QObject *parent = nullptr);
    virtual ~GreeterDisplayWayland();

    void start();

signals:
    void setOutputFinished();
    void setOutputStart(); // 复制模式下热插拔时隐藏greeter界面，避免闪屏，完成时显示

private:
    void setupRegistry(Registry *registry);
    void onDeviceChanged(OutputDevice *dev);
    void applyConfig(QString uuid);
    void applyDefault();
    QString getStdMonitorName(QByteArray edid);
    bool edidEqual(QByteArray edid1, QByteArray edid2);
    QString getOutputUuid(QString name, QString stdName, QByteArray edid);
    QString getOutputDeviceName(const QString& model, const QString& make);
    void setOutputs();
    QSize commonSizeForMirrorMode();

private:
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue;
    OutputConfiguration *m_pConf;
    OutputManagement *m_pManager;
    int m_displayMode;
    QJsonObject m_screensObj;
    QString m_removeUuid; // 防止多次移除事件
    QTimer *m_setOutputTimer;
};

#endif // GREETER_DISPLAY_WAYLAND_H
