// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHUTDOWNWIDGET
#define SHUTDOWNWIDGET

#include <functional>

#include "rounditembutton.h"
#include "sessionbasemodel.h"
#include "switchosinterface.h"
#include "systemmonitor.h"


class QVBoxLayout;
class QHBoxLayout;
class QStackedLayout;

DCORE_BEGIN_NAMESPACE
class DConfig;
DCORE_END_NAMESPACE

using HuaWeiSwitchOSInterface = com::huawei::switchos;

class ShutdownWidget: public QFrame
{
    Q_OBJECT
public:
    ShutdownWidget(QWidget* parent = nullptr);
    ~ShutdownWidget() override;
    void setModel(SessionBaseModel * const model);
    void onStatusChanged(SessionBaseModel::ModeStatus status);
    bool enableState(int settingsValue);

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
    int m_index = -1;
    bool m_switchUserEnable= false;
    QList<RoundItemButton *> m_btnList;
    QList<std::pair<std::function<void (QString)>, QString>> m_trList;
    SessionBaseModel* m_model = nullptr;
    QFrame* m_shutdownFrame = nullptr;
    SystemMonitor* m_systemMonitor = nullptr;
    QFrame* m_actionFrame = nullptr;
    QStackedLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_shutdownLayout = nullptr;
    QVBoxLayout* m_actionLayout = nullptr;
    RoundItemButton* m_currentSelectedBtn = nullptr;
    RoundItemButton* m_requireShutdownButton = nullptr;
    RoundItemButton* m_requireRestartButton = nullptr;
    RoundItemButton* m_requireSuspendButton = nullptr;
    RoundItemButton* m_requireHibernateButton = nullptr;
    RoundItemButton* m_requireLockButton = nullptr;
    RoundItemButton* m_requireLogoutButton = nullptr;
    RoundItemButton* m_requireSwitchUserBtn = nullptr;
    RoundItemButton* m_requireSwitchSystemBtn = nullptr;
    RoundItemButton* m_updateAndShutdownButton = nullptr;
    RoundItemButton* m_updateAndRebootButton = nullptr;
    HuaWeiSwitchOSInterface *m_switchosInterface = nullptr;
    SessionBaseModel::ModeStatus m_modeStatus;
    DTK_CORE_NAMESPACE::DConfig *m_dconfig = nullptr;
};

#endif // SHUTDOWNWIDGET
