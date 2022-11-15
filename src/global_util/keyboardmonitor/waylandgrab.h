// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef WAYLANDGRAB_H
#define WAYLANDGRAB_H

#include <QObject>

#include "wayland-xwayland-keyboard-grab-v1-client-protocol.h"

namespace QtWaylandClient {
    class QWaylandWindow;
}

class WaylandGrab : public QObject
{
    Q_OBJECT
public:
    explicit WaylandGrab(QObject *parent = nullptr);
    ~WaylandGrab();

private:
    void init();

    struct ::wl_seat*  m_wlSeat;
    struct zwp_xwayland_keyboard_grab_v1 *m_zxgm;
    QtWaylandClient::QWaylandWindow* m_waylandWindow;

public Q_SLOTS:
    void grab();
    void release();
private:
    QWidget* parentWidget;
};




#endif // WAYLANDGRAB_Hs
