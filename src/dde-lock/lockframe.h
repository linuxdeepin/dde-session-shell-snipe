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

const QString DBUS_LOCK_PATH = "/com/deepin/dde/lockFront";
const QString DBUS_LOCK_NAME = "com.deepin.dde.lockFront";

const QString DBUS_SHUTDOWN_PATH = "/com/deepin/dde/shutdownFront";
const QString DBUS_SHUTDOWN_NAME = "com.deepin.dde.shutdownFront";

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
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);

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
    static QPointer<WarningContent> m_warningContent;
    bool m_enablePowerOffKey;
    QTimer *m_autoExitTimer;
};

#endif // LOCKFRAME
