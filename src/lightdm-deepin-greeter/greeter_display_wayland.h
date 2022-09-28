// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
