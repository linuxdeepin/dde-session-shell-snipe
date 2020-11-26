/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
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

#ifndef SHUTDOWNFRAME
#define SHUTDOWNFRAME

#include <QFrame>

#include "src/widgets/fullscreenbackground.h"
#include "src/dde-shutdown/view/contentwidget.h"

#include <com_deepin_sessionmanager.h>

using SessionManagerInter = com::deepin::SessionManager;

class ShutdownFrontDBus;
class SessionBaseModel;
class DBusShutdownAgent;
class ShutdownFrame: public FullscreenBackground
{
    Q_OBJECT
public:
    ShutdownFrame(SessionBaseModel * const model, QWidget* parent = nullptr);
    ~ShutdownFrame();

Q_SIGNALS:
    void requestEnableHotzone(bool enable);
    void buttonClicked(const Actions action);

public slots:
    bool powerAction(const Actions action);
    void setConfirm(const bool confrim);
    void visibleChangedFrame(bool isVisible);

protected:
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;

private:
    SessionBaseModel *m_model;
    ContentWidget *m_shutdownFrame;
    SessionManagerInter *m_sessionInter;
};

class ShutdownFrontDBus : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.dde.shutdownFront")

public:
    ShutdownFrontDBus(DBusShutdownAgent *parent,SessionBaseModel* model);
    ~ShutdownFrontDBus();

    Q_SLOT void Shutdown();
    Q_SLOT void Restart();
    Q_SLOT void Logout();
    Q_SLOT void Suspend();
    Q_SLOT void Hibernate();
    Q_SLOT void SwitchUser();
    Q_SLOT void Show();

private:
    DBusShutdownAgent* m_parent;
    SessionBaseModel *m_model;
    SessionManagerInter *m_sessionInter;
};
#endif // SHUTDOWNFRAME

