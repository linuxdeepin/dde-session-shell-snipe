#ifndef LOCKCONTENT_H
#define LOCKCONTENT_H

#include <QWidget>
#include <memory>
#include <com_deepin_daemon_accounts.h>

#include "sessionbasewindow.h"
#include "sessionbasemodel.h"
#include "src/widgets/mediawidget.h"
#include "src/widgets/wirelesswidget.h"

using AccountsInter = com::deepin::daemon::Accounts;

class ControlWidget;
class UserInputWidget;
class User;
class ShutdownWidget;
class LogoWidget;
class TimeWidget;
class UserLoginInfo;

class LockContent : public SessionBaseWindow
{
    Q_OBJECT
public:
    explicit LockContent(SessionBaseModel *const model, QWidget *parent = nullptr);

    virtual void onCurrentUserChanged(std::shared_ptr<User> user);
    virtual void onStatusChanged(SessionBaseModel::ModeStatus status);
    virtual void restoreCenterContent();
    virtual void restoreMode();

signals:
    void requestBackground(const QString &path);
    void requestAuthUser(const QString &password);
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &value);
    void unlockActionFinish();

public slots:
    void pushPasswordFrame();
    void pushUserFrame();
    void pushConfirmFrame();
    void pushShutdownFrame();
    void setMPRISEnable(const bool state);
    void beforeUnlockAction(bool is_finish);

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) override;

protected:
    void updateTimeFormat(bool use24);
    void toggleVirtualKB();
    void updateVirtualKBPosition();
    void onUserListChanged(QList<std::shared_ptr<User>> list);
    void tryGrabKeyboard();

protected:
    SessionBaseModel *m_model;
    ControlWidget *m_controlWidget;
    ShutdownWidget *m_shutdownFrame;
    QWidget *m_virtualKB;
    std::shared_ptr<User> m_user;
    QList<QMetaObject::Connection> m_currentUserConnects;
    QTranslator *m_translator;
    LogoWidget *m_logoWidget;
    TimeWidget *m_timeWidget;
    MediaWidget *m_mediaWidget;
    UserLoginInfo *m_userLoginInfo;
    int m_failures = 0;
    AccountsInter *    m_accountsInter;
};

#endif // LOCKCONTENT_H
