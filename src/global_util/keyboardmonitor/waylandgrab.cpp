// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "waylandgrab.h"

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformintegration.h>
#include <private/qguiapplication_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwaylandsurface_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>

#include <QWidget>
#include <QDebug>

static struct zwp_xwayland_keyboard_grab_manager_v1* xkgm = nullptr;

void MyRegistryListener(void *data,
                                 struct wl_registry *registry,
                                 uint32_t id,
                                 const QString &interface,
                                 uint32_t version)
{
    Q_UNUSED(data);
    Q_UNUSED(version);
    if(interface == QLatin1String("zwp_xwayland_keyboard_grab_manager_v1")){
        xkgm = static_cast<struct zwp_xwayland_keyboard_grab_manager_v1 *>(wl_registry_bind(
                    registry,id,&zwp_xwayland_keyboard_grab_manager_v1_interface,version));
    }
}

WaylandGrab::WaylandGrab(QObject *parent)
    : QObject(parent)
    , m_wlSeat(nullptr)
    , m_zxgm(nullptr)
    , m_waylandWindow(nullptr)
{
    init();
}

WaylandGrab::~WaylandGrab()
{
    if (m_zxgm) {
        zwp_xwayland_keyboard_grab_v1_destroy(m_zxgm);
        m_zxgm = nullptr;
    }
    if (xkgm) {
        zwp_xwayland_keyboard_grab_manager_v1_destroy(xkgm);
        xkgm = nullptr;
    }
}

void WaylandGrab::init()
{
    parentWidget = qobject_cast<QWidget*>(parent());
    if (!parentWidget) {
        qWarning() << "Parent is nullptr";
        return;
    }
    parentWidget->createWinId();
    if (!parentWidget->windowHandle()) {
        qWarning() << "Parent's handle is nullptr";
        return;
    }

}

void WaylandGrab::grab()
{
    qInfo() << Q_FUNC_INFO;

    m_waylandWindow = static_cast<QtWaylandClient::QWaylandWindow* >(parentWidget->windowHandle()->handle());
    QtWaylandClient::QWaylandIntegration * waylandIntegration = static_cast<QtWaylandClient::QWaylandIntegration* >(
                QGuiApplicationPrivate::platformIntegration());
    m_wlSeat = waylandIntegration->display()->currentInputDevice()->wl_seat();
    waylandIntegration->display()->addRegistryListener(MyRegistryListener, nullptr);
    if (!m_waylandWindow || !m_wlSeat || !m_waylandWindow->wlSurface()) {
        if (!m_waylandWindow)
            qWarning() << "wayland window is nullptr";
        if (!m_wlSeat)
            qWarning() << "wlSeat is nullptr";
        if (!m_waylandWindow->wlSurface())
            qWarning() << "wlSurface is nullptr";

        return;
    }

    m_zxgm = zwp_xwayland_keyboard_grab_manager_v1_grab_keyboard(xkgm, m_waylandWindow->wlSurface(), m_wlSeat);
}

void WaylandGrab::release()
{
    qInfo() << Q_FUNC_INFO;
    if (!m_zxgm) {
        qWarning() << "zxgm is nullptr";
        return;
    }

    zwp_xwayland_keyboard_grab_v1_destroy(m_zxgm);
    m_zxgm = nullptr;
}
