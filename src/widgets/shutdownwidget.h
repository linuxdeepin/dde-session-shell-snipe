// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHUTDOWNWIDGET
#define SHUTDOWNWIDGET

#include <QFrame>

#include <functional>
#include <dtkcore_global.h>

#include "util_updateui.h"
#include "rounditembutton.h"
#include "sessionbasemodel.h"
#include "switchos_interface.h"
#include "systemmonitor.h"

class ShutdownWidget: public QFrame
{
    Q_OBJECT
public:
    ShutdownWidget(QWidget* parent = nullptr);
    ~ShutdownWidget() override;
    void setModel(SessionBaseModel * const model);
    void onStatusChanged(SessionBaseModel::ModeStatus status);
#ifndef ENABLE_DSS_SNIPE
    bool enableState(const QString &gsettingsValue);
#else
    bool enableState(int settingsValue);
#endif
    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

public slots:
    void leftKeySwitch();
    void rightKeySwitch();
    void runSystemMonitor();
    void recoveryLayout();
    void onRequirePowerAction(SessionBaseModel::PowerAction powerAction, bool needConfirm);
    void setUserSwitchEnable(bool enable);
    void onEnable(const QString &key, bool enable);
    void updateLocale(std::shared_ptr<User> user);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void initConnect();
    void updateTr(RoundItemButton * widget, const QString &tr);
    void enterKeyPushed();
    void enableHibernateBtn(bool enable);
    void enableSleepBtn(bool enable);
    void setButtonsVisible();

private:
    int m_index;
    bool m_switchUserEnable= false;
    QList<RoundItemButton *> m_btnList;
    QList<std::pair<std::function<void (QString)>, QString>> m_trList;
    SessionBaseModel* m_model;
    QFrame* m_shutdownFrame = nullptr;
    SystemMonitor* m_systemMonitor = nullptr;
    QFrame* m_actionFrame = nullptr;
    QStackedLayout* m_mainLayout;
    QHBoxLayout* m_shutdownLayout;
    QVBoxLayout* m_actionLayout;
    RoundItemButton* m_currentSelectedBtn;
    RoundItemButton* m_requireShutdownButton;
    RoundItemButton* m_requireRestartButton;
    RoundItemButton* m_requireSuspendButton;
    RoundItemButton* m_requireHibernateButton;
    RoundItemButton* m_requireLockButton;
    RoundItemButton* m_requireLogoutButton;
    RoundItemButton* m_requireSwitchUserBtn;
    RoundItemButton* m_requireSwitchSystemBtn;
    RoundItemButton* m_updateAndShutdownButton;
    RoundItemButton* m_updateAndRebootButton;
    HuaWeiSwitchOSInterface *m_switchosInterface;
    SessionBaseModel::ModeStatus m_modeStatus;
};

#endif // SHUTDOWNWIDGET
