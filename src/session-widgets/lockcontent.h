#ifndef LOCKCONTENT_H
#define LOCKCONTENT_H

#include <QWidget>
#include <QLocalServer>

#include <memory>

#include "mediawidget.h"
#include "sessionbasemodel.h"
#include "sessionbasewindow.h"
#include "systemmonitor.h"

#include <com_deepin_wm.h>

class AuthWidget;
class MFAWidget;
class SFAWidget;
class UserFrameList;
class ControlWidget;
class UserInputWidget;
class User;
class ShutdownWidget;
class LogoWidget;
class TimeWidget;

class LockContent : public SessionBaseWindow
{
    Q_OBJECT

public:
    explicit LockContent(SessionBaseModel *const model, QWidget *parent = nullptr);

    virtual void onCurrentUserChanged(std::shared_ptr<User> user);
    virtual void onStatusChanged(SessionBaseModel::ModeStatus status);
    virtual void restoreMode();
    void updateGreeterBackgroundPath(const QString &path);
    void updateDesktopBackgroundPath(const QString &path);

signals:
    void requestBackground(const QString &path);
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &value);
    void unlockActionFinish();

    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();

    void requestCheckAccount(const QString &account);
    void requestLockFrameHide();

public slots:
    void pushPasswordFrame();
    void pushUserFrame();
    void pushConfirmFrame();
    void pushShutdownFrame();
    void setMPRISEnable(const bool state);
    void onNewConnection();
    void onDisConnect();

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

protected:
    void updateTimeFormat(bool use24);
    void toggleVirtualKB();
    void showModule(const QString &name);
    void updateVirtualKBPosition();
    void onUserListChanged(QList<std::shared_ptr<User>> list);
    void tryGrabKeyboard(bool exitIfFalied = true);
    void currentWorkspaceChanged();
    void updateWallpaper(const QString &path);
    void refreshBackground(SessionBaseModel::ModeStatus status);
    void refreshLayout(SessionBaseModel::ModeStatus status);

    void initUI();
    void initConnections();
    void initMFAWidget();
    void initSFAWidget();
    void initUserListWidget();

protected:
    SessionBaseModel *m_model;
    ControlWidget *m_controlWidget;
    ShutdownWidget *m_shutdownFrame;
    QWidget *m_virtualKB;
    std::shared_ptr<User> m_user;
    QList<QMetaObject::Connection> m_currentUserConnects;
    LogoWidget *m_logoWidget;
    TimeWidget *m_timeWidget;
    MediaWidget *m_mediaWidget = nullptr;
    com::deepin::wm *m_wmInter;
    QWidget *m_loginWidget;
    QMap<QString, QWidget *> m_centeralWidgets;

    SFAWidget *m_sfaWidget;
    MFAWidget *m_mfaWidget;
    AuthWidget *m_authWidget;
    UserFrameList *m_userListWidget;

    int m_failures = 0;
    QLocalServer *m_localServer;
};

#endif // LOCKCONTENT_H
