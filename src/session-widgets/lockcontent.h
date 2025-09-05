// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOCKCONTENT_H
#define LOCKCONTENT_H

#include "mediawidget.h"
#include "sessionbasemodel.h"
#include "sessionbasewindow.h"
#include "centertopwidget.h"
#include "popupwindow.h"

#include <QWidget>
#include <QLocalServer>

#include <memory>
#ifndef ENABLE_DSS_SNIPE
#include <com_deepin_wm.h>
#else
#include "wminterface.h"
#endif

class AuthWidget;
class MFAWidget;
class SFAWidget;
class UserFrameList;
class ControlWidget;
class User;
class ShutdownWidget;
class LogoWidget;
class FullManagedAuthWidget;

class LockContent : public SessionBaseWindow
{
    Q_OBJECT

public:
    explicit LockContent(QWidget *parent = nullptr);
    static LockContent *instance();
    void init(SessionBaseModel *model);
    virtual void onCurrentUserChanged(std::shared_ptr<User> user);
    virtual void onStatusChanged(SessionBaseModel::ModeStatus status);
    virtual void restoreMode();
    void updateGreeterBackgroundPath(const QString &path);
    void updateDesktopBackgroundPath(const QString &path);
    void hideEvent(QHideEvent *event) override;
    static void OnDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

signals:
    void requestBackground(const QString &path);
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &value);
    void unlockActionFinish();
    void requestStartAuthentication(const QString &account, const AuthFlags authType);
    void sendTokenToAuth(const QString &account, const AuthType authType, const QString &token);
    void requestEndAuthentication(const QString &account, const AuthFlags authType);
    void authFinished();
    void requestCheckSameNameAccount(const QString &account, bool switchUser);
    void requestCheckAccount(const QString &account, bool switchUser);
    void requestLockFrameHide();
    void parentChanged();
    void noPasswordLoginChanged(const QString &account, bool noPassword);
    void requestLockStateChange(const bool locked);

public slots:
    void pushPasswordFrame();
    void pushUserFrame();
    void pushConfirmFrame();
    void pushShutdownFrame();
    void setMPRISEnable(const bool state);
    void onNewConnection();
    void onDisConnect();
    void showUserList();
    void showLockScreen();
    void showShutdown();

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *watched, QEvent *e) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;

protected:
    void toggleVirtualKB();
    void showModule(const QString &name, const bool callShowForce);
    void updateVirtualKBPosition();
    void onUserListChanged(QList<std::shared_ptr<User>> list);
    void tryGrabKeyboard(bool exitIfFalied = true);
    void currentWorkspaceChanged();
    void updateWallpaper(const QString &path);
    void refreshBackground(SessionBaseModel::ModeStatus status);
    void refreshLayout(SessionBaseModel::ModeStatus status);
    void showTrayPopup(QWidget *trayWidget, QWidget *contentWidget, const bool callShowForce);

    void initUI();
    void initConnections();
    void initMFAWidget();
    void initSFAWidget();
    void initFMAWidget();
    void initUserListWidget();
    void enableSystemShortcut(const QStringList &shortcuts, bool enabled, bool isPersistent);
    QString getCurrentKBLayout() const;
    void setKBLayout(const QString &layout);

protected:
    SessionBaseModel *m_model = nullptr;
    ControlWidget *m_controlWidget = nullptr;
    CenterTopWidget *m_centerTopWidget = nullptr;
    ShutdownWidget *m_shutdownFrame = nullptr;
    QPointer<QWidget> m_virtualKB = nullptr;
    std::shared_ptr<User> m_user;
    QList<QMetaObject::Connection> m_currentUserConnects;
    LogoWidget *m_logoWidget = nullptr;
    MediaWidget *m_mediaWidget = nullptr;
    com::deepin::wm *m_wmInter = nullptr;
    QWidget *m_loginWidget = nullptr;

    SFAWidget *m_sfaWidget = nullptr;
    MFAWidget *m_mfaWidget = nullptr;
    AuthWidget *m_authWidget = nullptr;
    FullManagedAuthWidget *m_fmaWidget = nullptr;
    UserFrameList *m_userListWidget = nullptr;

    int m_failures = 0;
    QLocalServer *m_localServer = nullptr;
    SessionBaseModel::ModeStatus m_currentModeStatus;
    bool m_initialized = false;
    bool m_isUserSwitchVisible = false;
    PopupWindow *m_popWin = nullptr;
    QPointer<QWidget> m_currentTray = nullptr;

    bool m_isPANGUCpu = false;
    bool m_MPRISEnable = false;
    bool m_showMediaWidget = false;
    bool m_hasResetPasswordDialog = false;

    QString m_originalKBLayout;
};

#endif // LOCKCONTENT_H
