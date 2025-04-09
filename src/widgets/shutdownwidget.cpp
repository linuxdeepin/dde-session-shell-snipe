// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shutdownwidget.h"
#include "../global_util/gsettingwatcher.h"
#include "dconfig_helper.h"

#ifdef ENABLE_DSS_SNIPE
#include "dconfigwatcher.h"
#endif

#if 0 // storage i10n
QT_TRANSLATE_NOOP("ShutdownWidget", "Shut down"),
QT_TRANSLATE_NOOP("ShutdownWidget", "Reboot"),
QT_TRANSLATE_NOOP("ShutdownWidget", "Suspend"),
QT_TRANSLATE_NOOP("ShutdownWidget", "Hibernate")
#endif

const QString LASTORE_DCONFIG_NAME = "org.deepin.lastore";
const QString LASTORE_DAEMON_STATUS = "lastore-daemon-status";
const int IS_UPDATE_READY       = 1 << 0;
const int IS_UPDATE_DISABLED    = 1 << 1;
const int IS_UPDATE_FORCE       = 1 << 2;

DCORE_USE_NAMESPACE

ShutdownWidget::ShutdownWidget(QWidget *parent)
    : QFrame(parent)
    , m_index(-1)
    , m_model(nullptr)
    , m_shutdownFrame(nullptr)
    , m_systemMonitor(nullptr)
    , m_actionFrame(nullptr)
    , m_mainLayout(nullptr)
    , m_shutdownLayout(nullptr)
    , m_actionLayout(nullptr)
    , m_currentSelectedBtn(nullptr)
    , m_requireShutdownButton(nullptr)
    , m_requireRestartButton(nullptr)
    , m_requireSuspendButton(nullptr)
    , m_requireHibernateButton(nullptr)
    , m_requireLockButton(nullptr)
    , m_requireLogoutButton(nullptr)
    , m_requireSwitchUserBtn(nullptr)
    , m_requireSwitchSystemBtn(nullptr)
    , m_updateAndShutdownButton(nullptr)
    , m_updateAndRebootButton(nullptr)
    , m_switchosInterface(new HuaWeiSwitchOSInterface("com.huawei", "/com/huawei/switchos", QDBusConnection::sessionBus(), this))
    , m_modeStatus(SessionBaseModel::ModeStatus::NoStatus)
{
    initUI();
    initConnect();

#ifndef ENABLE_DSS_SNIPE
    onEnable("systemShutdown", enableState(GSettingWatcher::instance()->getStatus("systemShutdown")));
    onEnable("systemSuspend", enableState(GSettingWatcher::instance()->getStatus("systemSuspend")));
    onEnable("systemHibernate", enableState(GSettingWatcher::instance()->getStatus("systemHibernate")));
    onEnable("systemLock", enableState(GSettingWatcher::instance()->getStatus("systemLock")));
    onEnable("systemReboot", enableState(GSettingWatcher::instance()->getStatus("systemReboot")));
#else
    onEnable("systemShutdown", enableState(DConfigWatcher::instance()->getStatus("systemShutdown")));
    onEnable("systemSuspend", enableState(DConfigWatcher::instance()->getStatus("systemSuspend")));
    onEnable("systemHibernate", enableState(DConfigWatcher::instance()->getStatus("systemHibernate")));
    onEnable("systemLock", enableState(DConfigWatcher::instance()->getStatus("systemLock")));
    onEnable("systemReboot", enableState(DConfigWatcher::instance()->getStatus("systemReboot")));
#endif

    installEventFilter(this);
}

ShutdownWidget::~ShutdownWidget()
{
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->erase("systemSuspend");
    GSettingWatcher::instance()->erase("systemHibernate");
    GSettingWatcher::instance()->erase("systemShutdown");
    GSettingWatcher::instance()->erase("systemLock");
    GSettingWatcher::instance()->erase("systemReboot");
#else
    DConfigWatcher::instance()->erase("systemSuspend");
    DConfigWatcher::instance()->erase("systemHibernate");
    DConfigWatcher::instance()->erase("systemShutdown");
    DConfigWatcher::instance()->erase("systemLock");
    DConfigWatcher::instance()->erase("systemReboot");
#endif
}

void ShutdownWidget::initConnect()
{
    connect(m_requireRestartButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireRestartButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireRestart, false);
    });
    connect(m_requireShutdownButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireShutdownButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, false);
    });
    connect(m_requireSuspendButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireSuspendButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireSuspend, false);
    });
    connect(m_requireHibernateButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireHibernateButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireHibernate, false);
    });
    connect(m_requireLockButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireLockButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireLock, false);
    });
    connect(m_requireSwitchUserBtn, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireSwitchUserBtn;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireSwitchUser, false);
    });
    if (m_requireSwitchSystemBtn) {
        connect(m_requireSwitchSystemBtn, &RoundItemButton::clicked, this, [this] {
            m_currentSelectedBtn = m_requireSwitchSystemBtn;
            onRequirePowerAction(SessionBaseModel::PowerAction::RequireSwitchSystem, false);
        });
    }
    connect(m_requireLogoutButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_requireLogoutButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireLogout, false);
    });
    connect(m_updateAndRebootButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_updateAndRebootButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireUpdateRestart, false);
    });
    connect(m_updateAndShutdownButton, &RoundItemButton::clicked, this, [this] {
        m_currentSelectedBtn = m_updateAndShutdownButton;
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireUpdateShutdown, false);
    });
#ifndef ENABLE_DSS_SNIPE
    connect(GSettingWatcher::instance(), &GSettingWatcher::enableChanged, this, &ShutdownWidget::onEnable);
#else
    connect(DConfigWatcher::instance(), &DConfigWatcher::enableChanged, this, &ShutdownWidget::onEnable);
#endif

    if (m_systemMonitor) {
        connect(m_systemMonitor, &SystemMonitor::requestAction, this, &ShutdownWidget::runSystemMonitor);
    }

    DConfigHelper::instance()->bind(this, "hideLogoutButton", &ShutdownWidget::onDConfigPropertyChanged);
}

void ShutdownWidget::updateTr(RoundItemButton *widget, const QString &tr)
{
    m_trList << std::pair<std::function<void (QString)>, QString>(std::bind(&RoundItemButton::setText, widget, std::placeholders::_1), tr);
}

#ifndef ENABLE_DSS_SNIPE
bool ShutdownWidget::enableState(const QString &gsettingsValue)
{
    if ("Disabled" == gsettingsValue)
        return false;
    else
        return true;
}
#else
bool ShutdownWidget::enableState(int settingsValue)
{
    if (Status_Disabled == settingsValue)
        return false;

    return true;
}
#endif

void ShutdownWidget::onEnable(const QString &key, bool enable)
{
    if ("systemShutdown" == key) {
        m_requireShutdownButton->setDisabled(!enable);
    } else if ("systemSuspend" == key) {
        m_requireSuspendButton->setDisabled(!enable);
        m_requireSuspendButton->setCheckable(enable);
    } else if ("systemHibernate" == key) {
        m_requireHibernateButton->setDisabled(!enable);
        m_requireHibernateButton->setCheckable(enable);
    } else if ("systemLock" == key) {
        m_requireLockButton->setDisabled(!enable);
    } else if ("systemReboot" == key) {
        m_requireRestartButton->setDisabled(!enable);
    }
}

void ShutdownWidget::enterKeyPushed()
{
    if (m_systemMonitor && m_systemMonitor->state() == SystemMonitor::Enter) {
        runSystemMonitor();
        return;
    }

    // 在按回车键时，若m_currentSelectedBtn不存在，则退出
    if (!m_currentSelectedBtn || !m_currentSelectedBtn->isEnabled() || !m_currentSelectedBtn->isVisible())
        return;

    if (m_currentSelectedBtn == m_requireShutdownButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireShutdown, false);
    else if (m_currentSelectedBtn == m_requireRestartButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireRestart, false);
    else if (m_currentSelectedBtn == m_requireSuspendButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireSuspend, false);
    else if (m_currentSelectedBtn == m_requireHibernateButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireHibernate, false);
    else if (m_currentSelectedBtn == m_requireLockButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireLock, false);
    else if (m_currentSelectedBtn == m_requireSwitchUserBtn)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireSwitchUser, false);
    else if (m_currentSelectedBtn == m_requireSwitchSystemBtn)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireSwitchSystem, false);
    else if (m_currentSelectedBtn == m_requireLogoutButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireLogout, false);
    else if (m_currentSelectedBtn == m_updateAndShutdownButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireUpdateShutdown, false);
    else if (m_currentSelectedBtn == m_updateAndRebootButton)
        onRequirePowerAction(SessionBaseModel::PowerAction::RequireUpdateRestart, false);
}

void ShutdownWidget::enableHibernateBtn(bool enable)
{
#ifndef ENABLE_DSS_SNIPE
    m_requireHibernateButton->setVisible(enable && (GSettingWatcher::instance()->getStatus("systemHibernate") != "Hiden"));
#else
    m_requireHibernateButton->setVisible(enable && (DConfigWatcher::instance()->getStatus("systemHibernate") != Status_Hidden));
#endif
}

void ShutdownWidget::enableSleepBtn(bool enable)
{
#ifndef ENABLE_DSS_SNIPE
    m_requireSuspendButton->setVisible(enable && (GSettingWatcher::instance()->getStatus("systemSuspend") != "Hiden"));
#else
    m_requireSuspendButton->setVisible(enable && (DConfigWatcher::instance()->getStatus("systemSuspend") != Status_Hidden));
#endif
}

void ShutdownWidget::initUI()
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto setPic = [](RoundItemButton* button, const QString &pic) {
        button->setNormalPic(":/img/list_actions/" + pic + "_normal.svg");
        button->setHoverPic(":/img/list_actions/" + pic + "_hover.svg");
        button->setPressPic(":/img/list_actions/" + pic + "_press.svg");
    };

    m_requireShutdownButton = new RoundItemButton(this);
    setPic(m_requireShutdownButton, "poweroff");
    m_requireShutdownButton->setFocusPolicy(Qt::NoFocus);
    m_requireShutdownButton->setObjectName("RequireShutDownButton");
    m_requireShutdownButton->setAccessibleName("RequireShutDownButton");
    m_requireShutdownButton->setAutoExclusive(true);
    updateTr(m_requireShutdownButton, "Shut down");
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->bind("systemShutdown", m_requireShutdownButton);  // GSettings配置项
#else
    DConfigWatcher::instance()->bind("systemShutdown", m_requireShutdownButton);
#endif

    m_requireRestartButton = new RoundItemButton(tr("Reboot"), this);
    setPic(m_requireRestartButton, "reboot");
    m_requireRestartButton->setFocusPolicy(Qt::NoFocus);
    m_requireRestartButton->setObjectName("RequireRestartButton");
    m_requireRestartButton->setAccessibleName("RequireRestartButton");
    m_requireRestartButton->setAutoExclusive(true);
    updateTr(m_requireRestartButton, "Reboot");
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->bind("systemReboot", m_requireRestartButton);  // GSettings配置项
#else
    DConfigWatcher::instance()->bind("systemReboot", m_requireRestartButton);
#endif

    m_requireSuspendButton = new RoundItemButton(tr("Suspend"), this);
    setPic(m_requireSuspendButton, "suspend");
    m_requireSuspendButton->setFocusPolicy(Qt::NoFocus);
    m_requireSuspendButton->setObjectName("RequireSuspendButton");
    m_requireSuspendButton->setAccessibleName("RequireSuspendButton");
    m_requireSuspendButton->setAutoExclusive(true);
    updateTr(m_requireSuspendButton, "Suspend");
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->bind("systemSuspend", m_requireSuspendButton);  // GSettings配置项
#else
    DConfigWatcher::instance()->bind("systemSuspend", m_requireSuspendButton);
#endif

    m_requireHibernateButton = new RoundItemButton(tr("Hibernate"), this);
    setPic(m_requireHibernateButton, "sleep");
    m_requireHibernateButton->setFocusPolicy(Qt::NoFocus);
    m_requireHibernateButton->setAccessibleName("RequireHibernateButton");
    m_requireHibernateButton->setObjectName("RequireHibernateButton");
    m_requireHibernateButton->setAutoExclusive(true);
    updateTr(m_requireHibernateButton, "Hibernate");
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->bind("systemHibernate", m_requireHibernateButton);  // GSettings配置项
#else
    DConfigWatcher::instance()->bind("systemHibernate", m_requireHibernateButton);
#endif

    m_requireLockButton = new RoundItemButton(tr("Lock"));
    setPic(m_requireLockButton, "lock");
    m_requireLockButton->setFocusPolicy(Qt::NoFocus);
    m_requireLockButton->setAccessibleName("RequireLockButton");
    m_requireLockButton->setObjectName("RequireLockButton");
    m_requireLockButton->setAutoExclusive(true);
    updateTr(m_requireLockButton, "Lock");
#ifndef ENABLE_DSS_SNIPE
    GSettingWatcher::instance()->bind("systemLock", m_requireLockButton);  // GSettings配置项
#else
    DConfigWatcher::instance()->bind("systemLock", m_requireLockButton);
#endif

    m_requireLogoutButton = new RoundItemButton(tr("Log out"));
    setPic(m_requireLogoutButton, "logout");
    m_requireLogoutButton->setFocusPolicy(Qt::NoFocus);
    m_requireLogoutButton->setAccessibleName("RequireLogoutButton");
    m_requireLogoutButton->setObjectName("RequireLogoutButton");
    m_requireLogoutButton->setAutoExclusive(true);
    updateTr(m_requireLogoutButton, "Log out");

    m_requireSwitchUserBtn = new RoundItemButton(tr("Switch user"));
    setPic(m_requireSwitchUserBtn, "userswitch");
    m_requireSwitchUserBtn->setFocusPolicy(Qt::NoFocus);
    m_requireSwitchUserBtn->setAccessibleName("RequireSwitchUserButton");
    m_requireSwitchUserBtn->setObjectName("RequireSwitchUserButton");
    m_requireSwitchUserBtn->setAutoExclusive(true);
    updateTr(m_requireSwitchUserBtn, "Switch user");
    m_requireSwitchUserBtn->setVisible(false);

    if (m_switchosInterface->isDualOsSwitchAvail()) {
        m_requireSwitchSystemBtn = new RoundItemButton(tr("Switch system"));
        setPic(m_requireSwitchSystemBtn, "osswitch");
        m_requireSwitchSystemBtn->setAccessibleName("SwitchSystemButton");
        m_requireSwitchSystemBtn->setFocusPolicy(Qt::NoFocus);
        m_requireSwitchSystemBtn->setObjectName("RequireSwitchSystemButton");
        m_requireSwitchSystemBtn->setAutoExclusive(true);
        updateTr(m_requireSwitchSystemBtn, "Switch system");
    }

    m_updateAndShutdownButton = new RoundItemButton(tr("Update and Shut Down"));
    setPic(m_updateAndShutdownButton, "poweroff");
    m_updateAndShutdownButton->setFocusPolicy(Qt::NoFocus);
    m_updateAndShutdownButton->setAccessibleName("UpdateAndShutdownButton");
    m_updateAndShutdownButton->setObjectName("UpdateAndShutdownButton");
    m_updateAndShutdownButton->setAutoExclusive(true);
    m_updateAndShutdownButton->setVisible(false);

    m_updateAndRebootButton = new RoundItemButton(tr("Update and Reboot"));
    setPic(m_updateAndRebootButton, "reboot");
    m_updateAndRebootButton->setFocusPolicy(Qt::NoFocus);
    m_updateAndRebootButton->setAccessibleName("UpdateAndRebootButton");
    m_updateAndRebootButton->setObjectName("UpdateAndRebootButton");
    m_updateAndRebootButton->setAutoExclusive(true);
    m_updateAndRebootButton->setVisible(false);

    m_btnList.append(m_requireShutdownButton);
    m_btnList.append(m_updateAndShutdownButton);
    m_btnList.append(m_requireRestartButton);
    m_btnList.append(m_updateAndRebootButton);
    m_btnList.append(m_requireSuspendButton);
    m_btnList.append(m_requireHibernateButton);
    m_btnList.append(m_requireLockButton);
    m_btnList.append(m_requireSwitchUserBtn);
    if(m_requireSwitchSystemBtn) {
        m_btnList.append(m_requireSwitchSystemBtn);
    }
    m_btnList.append(m_requireLogoutButton);

    m_shutdownLayout = new QHBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_shutdownLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_shutdownLayout->setMargin(0);
#endif
    m_shutdownLayout->setSpacing(10);
    m_shutdownLayout->addStretch();
    m_shutdownLayout->addWidget(m_requireShutdownButton);
    m_shutdownLayout->addWidget(m_updateAndShutdownButton);
    m_shutdownLayout->addWidget(m_requireRestartButton);
    m_shutdownLayout->addWidget(m_updateAndRebootButton);
    m_shutdownLayout->addWidget(m_requireSuspendButton);
    m_shutdownLayout->addWidget(m_requireHibernateButton);
    m_shutdownLayout->addWidget(m_requireLockButton);
    m_shutdownLayout->addWidget(m_requireSwitchUserBtn);
    if(m_requireSwitchSystemBtn) {
        m_shutdownLayout->addWidget(m_requireSwitchSystemBtn);
    }
    m_shutdownLayout->addWidget(m_requireLogoutButton);
    m_shutdownLayout->addStretch(0);

    m_shutdownFrame = new QFrame;
    m_shutdownFrame->setAccessibleName("ShutdownFrame");
    m_shutdownFrame->setLayout(m_shutdownLayout);
    m_shutdownFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_actionLayout = new QVBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_actionLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_actionLayout->setMargin(0);
#endif
    m_actionLayout->setSpacing(10);
    m_actionLayout->addStretch();
    m_actionLayout->addWidget(m_shutdownFrame);
    m_actionLayout->addStretch();

    if (findValueByQSettings<bool>(DDESESSIONCC::session_ui_configs, "Shutdown", "enableSystemMonitor", true)) {
        if (!QStandardPaths::findExecutable("deepin-system-monitor").isEmpty()) {
            m_systemMonitor = new SystemMonitor;
            m_systemMonitor->setAccessibleName("SystemMonitor");
            m_systemMonitor->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_systemMonitor->setFocusPolicy(Qt::NoFocus);
            setFocusPolicy(Qt::StrongFocus);
            m_actionLayout->addWidget(m_systemMonitor);
            m_actionLayout->setAlignment(m_systemMonitor, Qt::AlignHCenter);
        }
    }

    m_actionFrame = new QFrame(this);
    m_actionFrame->setAccessibleName("ActionFrame");
    m_actionFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_actionFrame->setFocusPolicy(Qt::NoFocus);
    m_actionFrame->setLayout(m_actionLayout);

    m_mainLayout = new QStackedLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_mainLayout->setMargin(0);
#endif
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_actionFrame);
    setLayout(m_mainLayout);

    // updateStyle(":/skin/requireshutdown.qss", this);

    // refresh language
    for (auto it = m_trList.constBegin(); it != m_trList.constEnd(); ++it) {
        it->first(qApp->translate("ShutdownWidget", it->second.toUtf8()));
    }
}

void ShutdownWidget::leftKeySwitch()
{
    m_btnList.at(m_index)->updateState(RoundItemButton::Normal);
    if (m_index <= 0) {
        m_index = m_btnList.length() - 1;
    } else {
        m_index  -= 1;
    }

    for (int i = m_btnList.size(); i != 0; --i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_currentSelectedBtn = m_btnList.at(m_index);
    if (m_currentSelectedBtn)
        m_currentSelectedBtn->updateState(RoundItemButton::Checked);

    if (m_systemMonitor && m_systemMonitor->isVisible()) {
        m_systemMonitor->setState(SystemMonitor::Leave);
    }
}

void ShutdownWidget::rightKeySwitch()
{
    m_btnList.at(m_index)->updateState(RoundItemButton::Normal);

    if (m_index == m_btnList.size() - 1) {
        m_index = 0;
    } else {
        ++m_index;
    }

    for (int i = 0; i < m_btnList.size(); ++i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_currentSelectedBtn = m_btnList.at(m_index);
    m_currentSelectedBtn->updateState(RoundItemButton::Checked);

    if (m_systemMonitor && m_systemMonitor->isVisible()) {
        m_systemMonitor->setState(SystemMonitor::Leave);
    }
}

void ShutdownWidget::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    RoundItemButton *roundItemButton;
    if (m_modeStatus == status) {
        qCInfo(DDE_SHELL) << "Shutdown widget status not being changed";
        return;
    }
    m_modeStatus = status;
    setButtonsVisible();

    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
        // 如果用户点击了更新选项，那么直接开始进入更新
        if (m_model->updatePowerMode() != SessionBaseModel::UPM_None) {
            roundItemButton = m_model->updatePowerMode() == SessionBaseModel::UPM_UpdateAndShutdown ?
                m_updateAndShutdownButton : m_updateAndRebootButton;
            // 当前流程执行完毕后模拟点击按钮
            QTimer::singleShot(0, this, [roundItemButton] {
                Q_EMIT roundItemButton->clicked();
            });
        } else {
            roundItemButton = m_requireLockButton;
        }
    } else {
        roundItemButton = m_requireShutdownButton;
    }

    m_index = m_btnList.indexOf(roundItemButton);
    m_currentSelectedBtn = m_btnList.at(m_index);
    m_currentSelectedBtn->updateState(RoundItemButton::Checked);

    if (m_systemMonitor) {
        m_systemMonitor->setVisible(status == SessionBaseModel::ModeStatus::ShutDownMode);
    }
}

void ShutdownWidget::setButtonsVisible()
{
    // 根据lastore的lastore-daemon-status配置决定是否显示更新按钮
    const int lastoreDaemonStatus = DConfigHelper::instance()->getConfig(LASTORE_DCONFIG_NAME, LASTORE_DCONFIG_NAME, "", LASTORE_DAEMON_STATUS, 0).toInt();
    qCInfo(DDE_SHELL) << "Lastore daemon status: " << lastoreDaemonStatus;
    const bool isUpdateReady = lastoreDaemonStatus & IS_UPDATE_READY;
    const bool isUpdateDisabled = lastoreDaemonStatus & IS_UPDATE_DISABLED;
    const bool isUpdateForce = lastoreDaemonStatus & IS_UPDATE_FORCE;
    const bool isUpdateVisible = isUpdateReady && !isUpdateDisabled;

    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
#ifndef ENABLE_DSS_SNIPE
        m_requireLockButton->setVisible(GSettingWatcher::instance()->getStatus("systemLock") != "Hiden");
#else
        m_requireLockButton->setVisible(DConfigWatcher::instance()->getStatus("systemLock") != Status_Hidden);
#endif
        m_requireSwitchUserBtn->setVisible(m_switchUserEnable);
        if (m_requireSwitchSystemBtn) {
            m_requireSwitchSystemBtn->setVisible(true);
        }
        if (m_model->appType() == AppType::Lock) {
            const bool hideLogoutButton = DConfigHelper::instance()->getConfig("hideLogoutButton", false).toBool();
            m_requireLogoutButton->setVisible(!hideLogoutButton);
        }
        m_updateAndRebootButton->setVisible(isUpdateVisible);
        m_updateAndRebootButton->setRedPointVisible(isUpdateVisible);
        m_updateAndShutdownButton->setVisible(isUpdateVisible);
        m_updateAndShutdownButton->setRedPointVisible(isUpdateVisible);
    } else {
        m_requireLockButton->setVisible(false);
        m_requireSwitchUserBtn->setVisible(false);
        if (m_requireSwitchSystemBtn) {
            m_requireSwitchSystemBtn->setVisible(false);
        }
        m_requireLogoutButton->setVisible(false);
        m_updateAndShutdownButton->setVisible(false);
        m_updateAndRebootButton->setVisible(false);
    }

    if (isUpdateForce && isUpdateVisible) {
        qCInfo(DDE_SHELL) << "Force update is enabled, hide shutdown button and reboot button";
        m_requireShutdownButton->setVisible(false);
        m_requireRestartButton->setVisible(false);
    }
}

void ShutdownWidget::runSystemMonitor()
{
    QProcess::startDetached("deepin-system-monitor", {});

    if (m_systemMonitor) {
        m_systemMonitor->clearFocus();
        m_systemMonitor->setState(SystemMonitor::Leave);
    }

    m_model->setVisible(false);
}

void ShutdownWidget::recoveryLayout()
{
    // 关机或重启确认前会隐藏所有按钮,取消重启或关机后隐藏界面时重置按钮可见状态
    // 同时需要判断切换用户按钮是否允许可见
#ifndef ENABLE_DSS_SNIPE
    m_requireShutdownButton->setVisible(true && (GSettingWatcher::instance()->getStatus("systemShutdown") != "Hiden"));
    m_requireRestartButton->setVisible(GSettingWatcher::instance()->getStatus("systemReboot") != "Hiden");
#else
    m_requireShutdownButton->setVisible(true && (DConfigWatcher::instance()->getStatus("systemShutdown") != Status_Hidden));
    m_requireRestartButton->setVisible(DConfigWatcher::instance()->getStatus("systemReboot") != Status_Hidden);
#endif
    enableHibernateBtn(m_model->hasSwap());
    enableSleepBtn(m_model->canSleep());
    if (m_modeStatus == SessionBaseModel::ModeStatus::PowerMode)
        return;

    m_requireSwitchUserBtn->setVisible(m_switchUserEnable);
    m_mainLayout->setCurrentWidget(m_actionFrame);
    setFocusPolicy(Qt::StrongFocus);
}

void ShutdownWidget::onRequirePowerAction(SessionBaseModel::PowerAction powerAction, bool needConfirm)
{
    qCInfo(DDE_SHELL) << "Require power action: " << powerAction << ", need confirm: " << needConfirm;
    //锁屏或关机模式时，需要确认是否关机或检查是否有阻止关机
    if (m_model->appType() == Lock) {
        switch (powerAction) {
        //更新前检查是否允许关机
        case SessionBaseModel::PowerAction::RequireUpdateShutdown:
        case SessionBaseModel::PowerAction::RequireUpdateRestart:
        case SessionBaseModel::PowerAction::RequireShutdown:
        case SessionBaseModel::PowerAction::RequireRestart:
        case SessionBaseModel::PowerAction::RequireSwitchSystem:
        case SessionBaseModel::PowerAction::RequireLogout:
        case SessionBaseModel::PowerAction::RequireSuspend:
        case SessionBaseModel::PowerAction::RequireHibernate:
            emit m_model->shutdownInhibit(powerAction, needConfirm);
            break;
        default:
            m_model->setPowerAction(powerAction);
            break;
        }
    } else {
        //登录模式直接操作
        m_model->setPowerAction(powerAction);
    }
}

void ShutdownWidget::setUserSwitchEnable(bool enable)
{
    // 接收到用户列表变更信号号,记录切换用户是否允许可见,再根据当前是锁屏还是关机设置切换按钮可见状态
    if (m_switchUserEnable == enable) {
        return;
    }

    m_switchUserEnable = enable;
    m_requireSwitchUserBtn->setVisible(m_switchUserEnable && m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode);
}

bool ShutdownWidget::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Tab && m_modeStatus == SessionBaseModel::ShutDownMode) {
            if (m_systemMonitor && m_systemMonitor->isVisible() && m_currentSelectedBtn && m_currentSelectedBtn->isVisible()) {
                if (m_currentSelectedBtn->isChecked()) {
                    m_currentSelectedBtn->setChecked(false);
                    m_systemMonitor->setState(SystemMonitor::Enter);
                } else {
                    m_currentSelectedBtn->setChecked(true);
                    m_systemMonitor->setState(SystemMonitor::Leave);
                }
            }
            return true;
        }
    } else if (event->type() == QEvent::FocusOut && m_currentSelectedBtn && m_currentSelectedBtn->isVisible() && m_modeStatus != SessionBaseModel::ShutDownMode) {
        m_currentSelectedBtn->setChecked(false);
    } else if (event->type() == QEvent::FocusIn && m_currentSelectedBtn && m_currentSelectedBtn->isVisible() && m_modeStatus != SessionBaseModel::ShutDownMode) {
        m_currentSelectedBtn->setChecked(true);
    }

    if (event->type() == QEvent::Hide && m_modeStatus == SessionBaseModel::PowerMode) {
        m_currentSelectedBtn = m_requireShutdownButton;
    }

    return false;
}

void ShutdownWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        leftKeySwitch();
        break;
    case Qt::Key_Right:
        rightKeySwitch();
        break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        enterKeyPushed();
        break;
    default:
        break;
    }

    QFrame::keyPressEvent(event);
}

bool ShutdownWidget::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        for (auto it = m_trList.constBegin(); it != m_trList.constEnd(); ++it) {
            it->first(qApp->translate("ShutdownWidget", it->second.toUtf8()));
        }
    } else if (e->type() == QEvent::Hide) {
        // 隐藏的时候重置状态，避免下次进入的时候默认选中了更新按钮
        m_model->setUpdatePowerMode(SessionBaseModel::UPM_None);
    }

    return QFrame::event(e);
}

void ShutdownWidget::showEvent(QShowEvent *event)
{
    setFocus();
    QFrame::showEvent(event);
}

void ShutdownWidget::updateLocale(std::shared_ptr<User> user)
{
    Q_UNUSED(user)
    //只有登陆界面才需要根据系统切换语言
    if(qApp->applicationName() == "dde-lock") return;

    // refresh language
    for (auto it = m_trList.constBegin(); it != m_trList.constEnd(); ++it) {
        it->first(qApp->translate("ShutdownWidget", it->second.toUtf8()));
    }
}

void ShutdownWidget::setModel(SessionBaseModel *const model)
{
    m_model = model;

    connect(model, &SessionBaseModel::onRequirePowerAction, this, &ShutdownWidget::onRequirePowerAction);

    connect(model, &SessionBaseModel::onHasSwapChanged, this, &ShutdownWidget::enableHibernateBtn);
    enableHibernateBtn(model->hasSwap());

    connect(model, &SessionBaseModel::canSleepChanged, this, &ShutdownWidget::enableSleepBtn);
    connect(model, &SessionBaseModel::currentUserChanged, this, &ShutdownWidget::updateLocale);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [this](bool visible) {
        m_modeStatus = visible? m_modeStatus : SessionBaseModel::ModeStatus::NoStatus;
    });

    enableSleepBtn(model->canSleep());
}

void ShutdownWidget::onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<ShutdownWidget*>(objPtr);
    if (!obj)
        return;

    if (key == "hideLogoutButton" && obj->m_requireLogoutButton
        && obj->m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
        obj->m_requireLogoutButton->setVisible(!value.toBool());
    }
}
