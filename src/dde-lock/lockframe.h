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

#ifndef LOCKFRAME
#define LOCKFRAME

#include "fullscreenbackground.h"
#include "sessionbasemodel.h"

#include <QKeyEvent>
#include <QDBusConnection>
#include <QDBusAbstractAdaptor>
#include <QTimer>

#include <memory>

const QString DBUS_LOCK_PATH = "/com/deepin/dde/lockFront";
const QString DBUS_LOCK_NAME = "com.deepin.dde.lockFront";

const QString DBUS_SHUTDOWN_PATH = "/com/deepin/dde/shutdownFront";
const QString DBUS_SHUTDOWN_NAME = "com.deepin.dde.shutdownFront";

class DBusLockService;
class LockContent;
class WarningContent;
class User;
class LockFrame: public FullscreenBackground
{
    Q_OBJECT
public:
    LockFrame(SessionBaseModel *const model, QWidget *parent = nullptr);

signals:
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &layout);
    void requestEnableHotzone(bool disable);
    void sendKeyValue(QString keyValue);

    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();

public slots:
    void showUserList();
    void showLockScreen();
    void showShutdown();
    void shutdownInhibit(const SessionBaseModel::PowerAction action, bool needConfirm);
    void cancelShutdownInhibit(bool hideFrame);

protected:
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    bool handlePoweroffKey();

private:
    SessionBaseModel *m_model;
    LockContent *m_lockContent;
    WarningContent *m_warningContent;
    bool m_enablePowerOffKey;
    QTimer *m_autoExitTimer;
};

#endif // LOCKFRAME
