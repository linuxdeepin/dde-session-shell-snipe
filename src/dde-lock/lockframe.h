// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOCKFRAME
#define LOCKFRAME

#include "fullscreenbackground.h"
#include "sessionbasemodel.h"

#include <QKeyEvent>
#include <QDBusConnection>
#include <QDBusAbstractAdaptor>
#include <QTimer>

#include <memory>

const QString DBUS_LOCK_PATH = "/org/deepin/dde/LockFront1";
const QString DBUS_LOCK_NAME = "org.deepin.dde.LockFront1";

const QString DBUS_SHUTDOWN_PATH = "/org/deepin/dde/ShutdownFront1";
const QString DBUS_SHUTDOWN_NAME = "org.deepin.dde.ShutdownFront1";

class DBusLockService;
class LockContent;
class WarningContent;
class User;
class LockFrame: public FullScreenBackground
{
    Q_OBJECT
public:
    explicit LockFrame(SessionBaseModel *const model, QWidget *parent = nullptr);
    ~LockFrame();

signals:
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &layout);
    void requestEnableHotzone(bool disable);
    void sendKeyValue(QString keyValue);

    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const AuthType authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);

private slots:
    void prepareForSleep(bool isSleep);

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
    bool m_enablePowerOffKey;
    QTimer *m_autoExitTimer;
};

#endif // LOCKFRAME
