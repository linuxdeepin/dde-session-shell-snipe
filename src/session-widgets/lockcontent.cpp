// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockcontent.h"

#include "controlwidget.h"
#include "logowidget.h"
#include "mfa_widget.h"
#include "popupwindow.h"
#include "sessionbasemodel.h"
#include "sfa_widget.h"
#include "shutdownwidget.h"
#include "userframelist.h"
#include "virtualkbinstance.h"
#include "plugin_manager.h"
#include "fullmanagedauthwidget.h"
#include "keyboardmonitor.h"
#include "dconfig_helper.h"
#include "constants.h"
#include "mfasequencecontrol.h"

#include <DDBusSender>
#include <DConfig>

#include <QLocalSocket>
#include <QMouseEvent>

#include "dbusconstant.h"

using namespace dss;
using namespace dss::module;
DCORE_USE_NAMESPACE

LockContent::LockContent(QWidget *parent)
    : SessionBaseWindow(parent)
    , m_wmInter(new com::deepin::wm("com.deepin.wm", "/com/deepin/wm", QDBusConnection::sessionBus(), this))
    , m_localServer(new QLocalServer(this))
    , m_currentModeStatus(SessionBaseModel::ModeStatus::NoStatus)
    , m_showMediaWidget(DConfigHelper::instance()->getConfig(SHOW_MEDIA_WIDGET, false).toBool())
{

}

LockContent *LockContent::instance()
{
    static LockContent *lockContent = nullptr;
    if (!lockContent) {
        lockContent = new LockContent();
    }

    return lockContent;
}

void LockContent::init(SessionBaseModel *model)
{
    if (m_initialized) {
        qCWarning(DDE_SHELL) << "Has been initialized, don't do it again";
        return;
    }
    m_initialized = true;
    m_model = model;
    // 异步获取CPU硬件信息，判断是否为PANGU CPU
    // FIXME: CPU硬件信息可能会改且其它的机型也可能会有PANGU CPU，这里的判断不准确
    if (m_model->appType() == AuthCommon::Lock) {
        QDBusInterface interface(DSS_DBUS::systemInfoService,
            DSS_DBUS::systemInfoPath,
            "org.freedesktop.DBus.Properties",
            QDBusConnection::sessionBus(),
            this);
        QDBusPendingReply<QDBusVariant> reply = interface.asyncCall("Get", DSS_DBUS::systemInfoService, "CPUHardware");
        QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            QDBusPendingReply<QDBusVariant> reply = *watcher;
            if (reply.isValid()) {
                QString cpuHardware = reply.value().variant().toString();
                qCInfo(DDE_SHELL) << "Current cpu hardware:" << cpuHardware;
                if (cpuHardware.contains("PANGU")) {
                    m_isPANGUCpu = true;
                }
            } else {
                qCWarning(DDE_SHELL) << "Failed to get CPU hardware:" << reply.error().message();
            }
            watcher->deleteLater();
        });
    }
    // 在已显示关机或用户列表界面时，再插入另外一个显示器，会新构建一个LockContent，此时会设置为PasswordMode造成界面状态异常
    if (!m_model->visible()) {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
    }

    initUI();
    initConnections();

    if (model->appType() == AuthCommon::Lock) {
        setMPRISEnable(model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode);
    }

    QTimer::singleShot(0, this, [this] {
        onCurrentUserChanged(m_model->currentUser());
    });

    // 创建套接字连接，用于与重置密码弹窗通讯
    m_localServer->setMaxPendingConnections(1);
    m_localServer->setSocketOptions(QLocalServer::WorldAccessOption);
    // 将greeter和lock的服务名称分开
    // 如果服务相同，但是创建套接字文件的用户不一样，greeter和lock不能删除对方的套接字文件，造成锁屏无法监听服务。
    QString serverName = QString("GrabKeyboard_") + (m_model->appType() == AuthCommon::Login ? "greeter" : ("lock_" + m_model->currentUser()->name()));
    // 将之前的server删除，如果是旧文件，即使监听成功，客户端也无法连接。
    QLocalServer::removeServer(serverName);
    if (!m_localServer->listen(serverName)) { // 监听特定的连接
        qCWarning(DDE_SHELL) << "Listen local server failed!" << m_localServer->errorString();
    }

    DConfigHelper::instance()->bind(this, SHOW_MEDIA_WIDGET, &LockContent::OnDConfigPropertyChanged);
}

void LockContent::initUI()
{
    m_centerTopWidget = new CenterTopWidget(this);
    setCenterTopWidget(m_centerTopWidget);

    m_shutdownFrame = new ShutdownWidget;
    m_shutdownFrame->setAccessibleName("ShutdownFrame");
    m_shutdownFrame->setModel(m_model);

    m_logoWidget = new LogoWidget;
    m_logoWidget->setAccessibleName("LogoWidget");
    setLeftBottomWidget(m_logoWidget);

    m_controlWidget = new ControlWidget(m_model, this);
    m_controlWidget->setAccessibleName("ControlWidget");
    setRightBottomWidget(m_controlWidget);

    if (m_model->getAuthProperty().MFAFlag) {
        initMFAWidget();
    } else {
        initSFAWidget();
    }
    m_authWidget->hide();

    initUserListWidget();

    if (PluginManager::instance()->getFullManagedLoginPlugin() != nullptr) {
        initFMAWidget();
    }
}

void LockContent::initConnections()
{
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &LockContent::onCurrentUserChanged);
    connect(m_controlWidget, &ControlWidget::requestSwitchUser, this, [this] {
        if (!m_model->userlistVisible() && m_model->currentUser()->name() == "...") {
            emit requestSwitchToUser(m_model->currentUser());
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        } else {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
            emit requestEndAuthentication(m_model->currentUser()->name(), AuthCommon::AT_All);
        }
    });
    connect(m_controlWidget, &ControlWidget::requestShutdown, this, [this] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
    });
    connect(m_controlWidget, &ControlWidget::requestSwitchVirtualKB, this, &LockContent::toggleVirtualKB);
    connect(m_controlWidget, &ControlWidget::requestShowModule, this, &LockContent::showModule);
    connect(m_controlWidget, &ControlWidget::requestShowTrayModule, this, &LockContent::showTrayPopup);

    // 刷新背景单独与onStatusChanged信号连接，避免在showEvent事件时调用onStatusChanged而重复刷新背景，减少刷新次数
    connect(m_model, &SessionBaseModel::onStatusChanged, this, &LockContent::onStatusChanged);

    //在锁屏显示时，启动onboard进程，锁屏结束时结束onboard进程
    auto initVirtualKB = [&](bool hasVirtualKB) {
        if (hasVirtualKB && !m_virtualKB) {
            connect(&VirtualKBInstance::Instance(), &VirtualKBInstance::initFinished, this, [&] {
                m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
                m_controlWidget->setVirtualKBVisible(true);
            },
                    Qt::QueuedConnection);
            VirtualKBInstance::Instance().init();
        } else {
            VirtualKBInstance::Instance().stopVirtualKBProcess();
            m_virtualKB = nullptr;
            m_controlWidget->setVirtualKBVisible(false);
        }
    };

    connect(m_model, &SessionBaseModel::hasVirtualKBChanged, this, initVirtualKB, Qt::QueuedConnection);
    connect(m_model, &SessionBaseModel::userListChanged, this, &LockContent::onUserListChanged);
    connect(m_model, &SessionBaseModel::userListLoginedChanged, this, &LockContent::onUserListChanged);
    connect(m_model, &SessionBaseModel::authFinished, this, [this](bool successful) {
        if (successful)
            setVisible(false);
        restoreMode();
    });
    connect(m_model, &SessionBaseModel::MFAFlagChanged, this, [this](const bool isMFA) {
        isMFA ? initMFAWidget() : initSFAWidget();
        // 当前中间窗口为空或者中间窗口就是验证窗口的时候显示验证窗口
        if (!m_centerWidget || m_centerWidget == m_authWidget)
            setCenterContent(m_authWidget->getAuthWidget(), 0, Qt::AlignTop, calcTopSpacing(m_authWidget->getTopSpacing()));
    });

    connect(m_wmInter, &com::deepin::wm::WorkspaceSwitched, this, &LockContent::currentWorkspaceChanged);
    connect(m_localServer, &QLocalServer::newConnection, this, &LockContent::onNewConnection);
    connect(m_controlWidget, &ControlWidget::notifyKeyboardLayoutHidden, this, [this] {
        if (!m_model->isUseWayland() && isVisible() && window()->windowHandle()) {
            qCDebug(DDE_SHELL) << "Grab keyboard after keyboard layout hidden";
            window()->windowHandle()->setKeyboardGrabEnabled(true);
        }
    });

    connect(m_model, &SessionBaseModel::showUserList, this, &LockContent::showUserList);
    connect(m_model, &SessionBaseModel::showLockScreen, this, &LockContent::showLockScreen);
    connect(m_model, &SessionBaseModel::showShutdown, this, &LockContent::showShutdown);

    // 连续按两次电源键导致锁屏界面失焦，无法接收到event，需重新抓取焦点
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, [this] {
        tryGrabKeyboard();
    });
}

/**
 * @brief 初始化多因认证界面
 */
void LockContent::initMFAWidget()
{
    qCDebug(DDE_SHELL) << "Init MFA widget, sfa widget: " << m_sfaWidget << ", mfa widget: " << m_mfaWidget;
    if (m_sfaWidget) {
        m_sfaWidget->hide();
        delete m_sfaWidget;
        m_sfaWidget = nullptr;
    }
    if (m_mfaWidget) {
        m_authWidget = m_mfaWidget;
        return;
    }
    m_mfaWidget = new MFAWidget(this);
    m_mfaWidget->setModel(m_model);
    m_authWidget = m_mfaWidget;

    connect(m_mfaWidget, &MFAWidget::requestStartAuthentication, this, &LockContent::requestStartAuthentication);
    connect(m_mfaWidget, &MFAWidget::sendTokenToAuth, this, &LockContent::sendTokenToAuth);
    connect(m_mfaWidget, &MFAWidget::requestEndAuthentication, this, &LockContent::requestEndAuthentication);
    connect(m_mfaWidget, &MFAWidget::requestCheckAccount, this, &LockContent::requestCheckAccount);
    connect(m_mfaWidget, &MFAWidget::requestCheckSameNameAccount, this, &LockContent::requestCheckSameNameAccount);

    connect(&MFASequenceControl::instance(), &MFASequenceControl::currentUIAuthTypeChanged, this, [&](int authType) {
        Q_UNUSED(authType)
        // 仅在当前处于认证界面上刷新
        auto isMFA = m_model->getAuthProperty().MFAFlag;
        qCInfo(DDE_SHELL) << "model mfa flag" << isMFA;
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::PasswordMode && isMFA) {
            pushPasswordFrame();
        }
    });
}

/**
 * @brief 初始化单因认证界面
 */
void LockContent::initSFAWidget()
{
    if (m_mfaWidget) {
        m_mfaWidget->hide();
        delete m_mfaWidget;
        m_mfaWidget = nullptr;
    }
    if (m_sfaWidget) {
        m_authWidget = m_sfaWidget;
        return;
    }
    m_sfaWidget = new SFAWidget(this);
    m_sfaWidget->setModel(m_model);
    m_authWidget = m_sfaWidget;

    connect(m_sfaWidget, &SFAWidget::requestStartAuthentication, this, &LockContent::requestStartAuthentication);
    connect(m_sfaWidget, &SFAWidget::sendTokenToAuth, this, &LockContent::sendTokenToAuth);
    connect(m_sfaWidget, &SFAWidget::requestEndAuthentication, this, &LockContent::requestEndAuthentication);
    connect(m_sfaWidget, &SFAWidget::requestCheckAccount, this, &LockContent::requestCheckAccount);
    connect(m_sfaWidget, &SFAWidget::requestCheckSameNameAccount, this, &LockContent::requestCheckSameNameAccount);
    connect(m_sfaWidget, &SFAWidget::authFinished, this, &LockContent::authFinished);
    connect(m_sfaWidget, &SFAWidget::updateParentLayout, this, [this] {
        if (!m_sfaWidget->isVisible())
            return;
        m_centerSpacerItem->changeSize(0, calcTopSpacing(m_sfaWidget->getTopSpacing()));
        m_centerVLayout->invalidate();
    });
    connect(m_sfaWidget, &AuthWidget::noPasswordLoginChanged, this, &LockContent::noPasswordLoginChanged);
}

// init full managed plugin widget
void LockContent::initFMAWidget()
{
    if (m_fmaWidget) {
        m_fmaWidget->hide();
        delete m_fmaWidget;
        m_fmaWidget = nullptr;
    }

    m_fmaWidget = new FullManagedAuthWidget();
    m_fmaWidget->setModel(m_model);
    if (m_fmaWidget->isPluginLoaded()) {
        setFullManagedLoginWidget(m_fmaWidget);
    }
    connect(m_fmaWidget, &FullManagedAuthWidget::requestStartAuthentication, this, &LockContent::requestStartAuthentication);
    connect(m_fmaWidget, &FullManagedAuthWidget::sendTokenToAuth, this, &LockContent::sendTokenToAuth);
    connect(m_fmaWidget, &FullManagedAuthWidget::requestEndAuthentication, this, &LockContent::requestEndAuthentication);
    connect(m_fmaWidget, &FullManagedAuthWidget::requestCheckAccount, this, &LockContent::requestCheckAccount);
    connect(m_fmaWidget, &FullManagedAuthWidget::requestCheckSameNameAccount, this, &LockContent::requestCheckSameNameAccount);
    connect(m_fmaWidget, &FullManagedAuthWidget::authFinished, this, &LockContent::authFinished);
}

/**
 * @brief 初始化用户列表界面
 */
void LockContent::initUserListWidget()
{
    if (m_userListWidget) {
        return;
    }
    m_userListWidget = new UserFrameList(this);
    m_userListWidget->setModel(m_model);
    m_userListWidget->setVisible(false);

    connect(m_userListWidget, &UserFrameList::clicked, this, &LockContent::restoreMode);
    connect(m_userListWidget, &UserFrameList::requestSwitchUser, this, &LockContent::requestSwitchToUser);
}

void LockContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (user.get() == nullptr)
        return; // if dbus is async

    //如果是锁屏就用系统语言，如果是登陆界面就用用户语言
    auto locale = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : user->locale();
    m_logoWidget->updateLocale(locale);

    for (auto connect : m_currentUserConnects) {
        m_user.get()->disconnect(connect);
    }

    m_currentUserConnects.clear();

    m_user = user;

    m_currentUserConnects << connect(user.get(), &User::greeterBackgroundChanged, this, &LockContent::updateGreeterBackgroundPath, Qt::UniqueConnection)
                          << connect(user.get(), &User::desktopBackgroundChanged, this, &LockContent::updateDesktopBackgroundPath, Qt::UniqueConnection);

    m_centerTopWidget->setCurrentUser(user.get());
    m_logoWidget->updateLocale(locale);

    onUserListChanged(m_model->isServerModel() ? m_model->loginedUserList() : m_model->userList());
}

void LockContent::pushPasswordFrame()
{
    if (!m_model->userlistVisible()) {
        if (m_model->currentUser()->name() == "...") {
            m_controlWidget->setUserSwitchEnable(false);
        } else {
            m_controlWidget->setUserSwitchEnable(m_isUserSwitchVisible);
        }
    }

    setCenterContent(m_authWidget->getAuthWidget(), 0, Qt::AlignTop, calcTopSpacing(m_authWidget->getTopSpacing()));

    m_authWidget->syncResetPasswordUI();

    if (m_fmaWidget && !m_fmaWidget->isPluginLoaded()) {
        showDefaultFrame();
        return;
    }

    showPasswdFrame();
}

void LockContent::pushUserFrame()
{
    qCInfo(DDE_SHELL) << "Push user frame";

    if (!m_model->userlistVisible()) {
        std::shared_ptr<User> user_ptr = m_model->findUserByName("...");
        if (user_ptr) {
            m_controlWidget->setUserSwitchEnable(false);
            emit requestSwitchToUser(user_ptr);
            return;
        }
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        return;
    }

    if (m_model->isServerModel())
        m_controlWidget->setUserSwitchEnable(false);

    m_userListWidget->updateLayout(width());
    setCenterContent(m_userListWidget);

    auto config = PluginConfigMap::instance().getConfig();
    if (config) {
        m_controlWidget->setUserSwitchEnable(config->isUserSwitchButtonVisiable());
    } else {
        m_controlWidget->setUserSwitchEnable(m_isUserSwitchVisible);
    }

    showDefaultFrame();

    // fix: 解决用户界面多账户区域无焦点问题
    // showDefaultFrame() -> hideStackedWidgets() -> 会将焦点置为空
    // 导致默认用户无选中状态，多账户区域无键盘事件
    setFocus();

    // 用户列表界面显示时，设置锁屏状态为锁定
    // 主要是为了处理需求：切换用户界面属于进入锁屏，待机唤醒时仍进入锁屏界面，需输入密码进入系统。
    Q_EMIT requestLockStateChange(true);
}

void LockContent::pushConfirmFrame()
{
    setCenterContent(m_authWidget->getAuthWidget(), 0, Qt::AlignTop, calcTopSpacing(m_authWidget->getTopSpacing()));
    showDefaultFrame();
}

void LockContent::pushShutdownFrame()
{
    qCInfo(DDE_SHELL) << "Push shutdown frame";
    // 设置关机选项界面大小为中间区域的大小,并移动到左上角，避免显示后出现移动现象
    auto config = PluginConfigMap::instance().getConfig();
    if (config) {
        m_shutdownFrame->setUserSwitchEnable(config->isUserSwitchButtonVisiable());
        m_controlWidget->setUserSwitchEnable(config->isUserSwitchButtonVisiable());
    } else {
        m_shutdownFrame->setUserSwitchEnable(m_isUserSwitchVisible);
        m_controlWidget->setUserSwitchEnable(m_isUserSwitchVisible);
    }

    //设置关机选项界面大小为中间区域的大小,并移动到左上角，避免显示后出现移动现象
    m_shutdownFrame->onStatusChanged(m_model->currentModeState());
    setCenterContent(m_shutdownFrame);
    showDefaultFrame();
}

void LockContent::setMPRISEnable(const bool state)
{
    m_MPRISEnable = state;

    if (!m_showMediaWidget) {
        return;
    }

    if (!m_mediaWidget) {
        m_mediaWidget = new MediaWidget;
        m_mediaWidget->setAccessibleName("MediaWidget");
        m_mediaWidget->initMediaPlayer();
    }

    m_mediaWidget->setVisible(state);
    setCenterBottomWidget(m_mediaWidget);
}

void LockContent::enableSystemShortcut(const QStringList &shortcuts, bool enabled, bool isPersistent)
{
    QDBusInterface inter(DSS_DBUS::keybindingService, DSS_DBUS::keybindingPath, DSS_DBUS::keybindingService, QDBusConnection::sessionBus());
    QList<QVariant> argumentList;
    argumentList << QVariant::fromValue(shortcuts) << QVariant::fromValue(enabled) << QVariant::fromValue(isPersistent);
    inter.asyncCallWithArgumentList(QStringLiteral("EnableSystemShortcut"), argumentList);
}

void LockContent::onNewConnection()
{
    // 重置密码界面显示前需要隐藏插件右键菜单，避免抢占键盘
    Q_EMIT m_model->hidePluginMenu();

    // 重置密码程序启动连接成功锁屏界面才释放键盘，避免点击重置密码过程中使用快捷键切走锁屏
    if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(false)) {
        qCDebug(DDE_SHELL) << "Set keyboard grab enabled to false success!";
    }

    if (m_localServer->hasPendingConnections()) {
        QLocalSocket *socket = m_localServer->nextPendingConnection();
        connect(socket, &QLocalSocket::disconnected, this, &LockContent::onDisConnect);
    }

    // 仅在锁屏界面使用
    if (m_model->appType() == AuthCommon::Lock) {
        enableSystemShortcut(QStringList() << "screenshot"
                                           << "screenshot-window"
                                           << "screenshot-delayed"
                                           << "screenshot-ocr"
                                           << "screenshot-scroll"
                                           << "deepin-screen-recorder",
                             false, false);
    }
    m_hasResetPasswordDialog = true;
}

void LockContent::onDisConnect()
{
    if (m_authWidget) {
        m_authWidget->syncPasswordResetPasswordVisibleChanged(QVariant::fromValue(true));
        m_authWidget->syncResetPasswordUI();
    }
    // 这种情况下不必强制要求可以抓取到键盘，因为可能是网络弹窗抓取了键盘
    tryGrabKeyboard(false);
    enableSystemShortcut(QStringList() << "screenshot"
                                       << "screenshot-window"
                                       << "screenshot-delayed"
                                       << "screenshot-ocr"
                                       << "screenshot-scroll"
                                       << "deepin-screen-recorder",
                         true, false);
}

void LockContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    qCInfo(DDE_SHELL) << "Incoming status:" << status << ", current status:" << m_currentModeStatus;
    refreshLayout(status);

    // select plugin config
    if (m_fmaWidget && m_fmaWidget->isPluginLoaded()) {
        PluginConfigMap::instance().requestAddConfig(PluginConfigMap::ConfigIndex::FullManagePlugin, m_fmaWidget);
    } else {
        PluginConfigMap::instance().requestRemoveConfig(PluginConfigMap::ConfigIndex::FullManagePlugin);
    }

    if (m_model->isServerModel())
        onUserListChanged(m_model->loginedUserList());

    if (!m_isPANGUCpu && m_currentModeStatus == status)
        return;
    m_currentModeStatus = status;

    switch (status) {
    case SessionBaseModel::ModeStatus::PasswordMode:
        pushPasswordFrame();
        break;
    case SessionBaseModel::ModeStatus::ConfirmPasswordMode:
        pushConfirmFrame();
        break;
    case SessionBaseModel::ModeStatus::UserMode:
        pushUserFrame();
        break;
    case SessionBaseModel::ModeStatus::PowerMode:
    case SessionBaseModel::ModeStatus::ShutDownMode:
        pushShutdownFrame();
        break;
    default:
        break;
    }

    m_model->setAbortConfirm(status == SessionBaseModel::ModeStatus::ConfirmPasswordMode);
    if (status != SessionBaseModel::ModeStatus::ConfirmPasswordMode)
        m_model->setPowerAction(SessionBaseModel::PowerAction::None);
}

void LockContent::mouseReleaseEvent(QMouseEvent *event)
{
    // 如果是设置密码界面，不做处理
    if (m_model->currentModeState() == SessionBaseModel::ResetPasswdMode)
        return SessionBaseWindow::mouseReleaseEvent(event);

    //在关机界面没有点击按钮直接点击界面时，直接隐藏关机界面
    if (m_model->currentModeState() == SessionBaseModel::ShutDownMode) {
        m_model->setVisible(false);
    }

    if (m_model->currentModeState() == SessionBaseModel::UserMode
        || m_model->currentModeState() == SessionBaseModel::PowerMode) {

        // 在用户列表界面，用户达到一定数量需要支持滚动，触屏滚动容易触发回到当前用户界面
        // ，所以这个界面触屏点击空白处不退出用户列表界面
        if (event->source() == Qt::MouseEventSynthesizedByQt
                && m_model->currentModeState() == SessionBaseModel::UserMode) {
            return SessionBaseWindow::mouseReleaseEvent(event);
        }

        // 点击空白处的时候切换到当前用户，以开启验证。
        emit requestSwitchToUser(m_model->currentUser());
    }

    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

    SessionBaseWindow::mouseReleaseEvent(event);
}

void LockContent::showEvent(QShowEvent *event)
{
    onStatusChanged(m_model->currentModeState());
    // 重新设置一下焦点，否则用户列表界面切换屏幕的时候焦点异常
    if (m_centerWidget)
        m_centerWidget->setFocus();

    tryGrabKeyboard();
    QFrame::showEvent(event);
}

void LockContent::hideEvent(QHideEvent *event)
{
    m_shutdownFrame->recoveryLayout();
    QFrame::hideEvent(event);
}

void LockContent::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, [this] {
        if (m_virtualKB && m_virtualKB->isVisible()) {
            updateVirtualKBPosition();
        }
    });

    if (SessionBaseModel::PasswordMode == m_model->currentModeState() || (SessionBaseModel::ConfirmPasswordMode == m_model->currentModeState())) {
        m_centerSpacerItem->changeSize(0, calcTopSpacing(m_authWidget->getTopSpacing()));
        m_centerVLayout->invalidate();
        QTimer::singleShot(0, this, [ = ] {
            m_authWidget->updatePasswordErrortipUi();
        });
    }

    SessionBaseWindow::resizeEvent(event);
}

void LockContent::restoreMode()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
}

void LockContent::updateGreeterBackgroundPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    emit requestBackground(path);
}

void LockContent::updateDesktopBackgroundPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    emit requestBackground(path);
}

void LockContent::OnDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<LockContent*>(objPtr);
    if (!obj)
        return;

    if (key == SHOW_MEDIA_WIDGET) {
        if (value.toBool()) {
            if (!obj->m_mediaWidget) {
                obj->m_mediaWidget = new MediaWidget;
                obj->m_mediaWidget->setAccessibleName("MediaWidget");
                obj->m_mediaWidget->initMediaPlayer();
            }

            obj->m_mediaWidget->setVisible(obj->m_MPRISEnable);
            obj->setCenterBottomWidget(obj->m_mediaWidget);
        } else {
            if (obj->m_mediaWidget) {
                obj->m_mediaWidget->setVisible(false);
                obj->m_mediaWidget->deleteLater();
                obj->m_mediaWidget = nullptr;
            }
        }
    }
}

void LockContent::toggleVirtualKB()
{
    if (!m_virtualKB) {
        VirtualKBInstance::Instance();
        QTimer::singleShot(500, this, [this] {
            m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
            qCDebug(DDE_SHELL) << "Init virtual keyboard over: " << m_virtualKB;
            toggleVirtualKB();
        });
        return;
    }

    m_virtualKB->setParent(this);
    m_virtualKB->raise();
    // m_userLoginInfo->getUserLoginWidget()->setPassWordEditFocus();

    updateVirtualKBPosition();
    m_virtualKB->setVisible(!m_virtualKB->isVisible());
}

void LockContent::showModule(const QString &name, const bool callShowForce)
{
    PluginBase *plugin = PluginManager::instance()->findPlugin(name);
    if (!plugin) {
        return;
    }

    switch (plugin->type()) {
    case PluginBase::ModuleType::LoginType:
        m_loginWidget = plugin->content();
        setCenterContent(m_loginWidget);
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        break;
    case PluginBase::ModuleType::TrayType: {
        m_currentTray = m_controlWidget->getTray(name);
        if (!m_currentTray || !plugin->content()) {
            qCWarning(DDE_SHELL) << "TrayButton or plugin`s content is null";
            return;
        }
        showTrayPopup(m_currentTray, plugin->content(), callShowForce);
        break;
    }
    default:
        // 扩展插件类型 FullManagedLoginType，不在此处处理
        break;
    }
}

bool LockContent::eventFilter(QObject *watched, QEvent *e)
{
    // 点击插件弹窗以外区域，隐藏插件弹窗
    if (m_popWin && m_currentTray && m_popWin->isVisible()) {
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (!w)
            return false;
        const bool isChild = m_currentTray->findChildren<QWidget *>().contains(w);
        if ((watched == m_currentTray || isChild) && e->type() == QEvent::Enter) {
            Q_EMIT qobject_cast<FloatingButton *>(m_currentTray)->requestHideTips();
        } else if (watched != m_currentTray && !isChild && e->type() == QEvent::MouseButtonRelease) {
            if (!m_popWin->geometry().contains(this->mapFromGlobal(QCursor::pos()))) {
                m_popWin->hide();
            }
        }
    }

    return false;
}

bool LockContent::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        if (m_popWin && m_currentTray && m_popWin->isVisible()) {
            m_popWin->activateWindow();
        }
    }
    return SessionBaseWindow::event(event);
}

void LockContent::updateVirtualKBPosition()
{
    const QPoint point = mapToParent(QPoint((width() - m_virtualKB->width()) / 2, height() - m_virtualKB->height() - 50));
    m_virtualKB->move(point);
}

void LockContent::onUserListChanged(QList<std::shared_ptr<User>> list)
{
    const bool allowShowUserSwitchButton = m_model->allowShowUserSwitchButton();
    const bool alwaysShowUserSwitchButton = m_model->alwaysShowUserSwitchButton();
    bool haveLoginedUser = true;

    if (m_model->isServerModel() && m_model->appType() == AuthCommon::Login) {
        haveLoginedUser = !m_model->loginedUserList().isEmpty();
    }

    int userList = list.size();
    if (!m_model->isServerModel() && !m_model->userlistVisible()) {
        foreach (auto user, list) {
            if (user->name() == "...") {
                userList--;
            }
        }
    }

    bool enable = (alwaysShowUserSwitchButton || (allowShowUserSwitchButton && (userList > (m_model->isServerModel() ? 0 : 1)))) && haveLoginedUser;

    bool controlEnable = enable && (m_model->userlistVisible() || m_model->currentUser()->name() != "...");
    m_controlWidget->setUserSwitchEnable(controlEnable);
    m_shutdownFrame->setUserSwitchEnable(enable);

    m_isUserSwitchVisible = enable;
}

/**
 * @brief 抓取键盘,失败后继续抓取,最多尝试15次.
 *
 * @param exitIfFailed true: 发送失败通知，并隐藏锁屏。 false：不做任何处理。
 */
void LockContent::tryGrabKeyboard(bool exitIfFailed)
{
#ifndef QT_DEBUG
    if (!isVisible()) {
        return;
    }

    if (m_model->isUseWayland()) {
        static QDBusInterface *kwinInter = new QDBusInterface("org.kde.KWin", "/KWin", "org.kde.KWin", QDBusConnection::sessionBus());
        if (!kwinInter || !kwinInter->isValid()) {
            qCWarning(DDE_SHELL) << "Kwin interface is invalid";
            m_failures = 0;
            return;
        }
        // wayland下判断是否有应用发起grab
        QDBusReply<bool> reply = kwinInter->call("xwaylandGrabed");
        if (!reply.isValid() || !reply.value()) {
            m_failures = 0;
            return;
        }
    } else {
        if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(true)) {
            m_failures = 0;
            return;
        }
    }

    if (m_failures > 5) {
        qCWarning(DDE_SHELL) << "Trying ungrab keyboard in lock content";
        KeyboardMonitor::instance()->ungrabKeyboard();
    }

    m_failures++;

    if (m_failures == 15) {
        qCWarning(DDE_SHELL) << "Trying to grab keyboard has exceeded the upper limit";

        m_failures = 0;

        if (!exitIfFailed) {
            return;
        }

        if (m_hasResetPasswordDialog) {
            qCWarning(DDE_SHELL) << "There is a reset password dialog, can not grab keyboard";
            return;
        }

        DDBusSender()
            .service("org.freedesktop.Notifications")
            .path("/org/freedesktop/Notifications")
            .interface("org.freedesktop.Notifications")
            .method(QString("Notify"))
            .arg(tr("Lock Screen"))
            .arg(static_cast<uint>(0))
            .arg(QString(""))
            .arg(QString(""))
            .arg(tr("Failed to lock screen"))
            .arg(QStringList())
            .arg(QVariantMap())
            .arg(5000)
            .call();

        DDBusSender()
            .service(DSS_DBUS::screenSaveService)
            .path("/org/freedesktop/ScreenSaver")
            .interface("org.freedesktop.ScreenSaver")
            .method(QString("SimulateUserActivity"))
            .call();

        qCInfo(DDE_SHELL) << "Request hide lock frame";
        emit requestLockFrameHide();
        return;
    }

    QTimer::singleShot(100, this, [this, exitIfFailed] {
        tryGrabKeyboard(exitIfFailed);
    });
#endif
}

void LockContent::currentWorkspaceChanged()
{
    QDBusPendingCall call = m_wmInter->GetCurrentWorkspaceBackgroundForMonitor(QGuiApplication::primaryScreen()->name());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [=] {
        if (!call.isError()) {
            QDBusReply<QString> reply = call.reply();
            updateWallpaper(reply.value());
        } else {
            qCWarning(DDE_SHELL) << "Get current workspace background error: " << call.error().message();
            updateWallpaper("/usr/share/backgrounds/deepin/desktop.jpg");
        }
        watcher->deleteLater();
    });
}

void LockContent::updateWallpaper(const QString &path)
{
    const QUrl url(path);
    QString wallpaper = path;
    if (url.isLocalFile()) {
        wallpaper = url.path();
    }

    updateDesktopBackgroundPath(wallpaper);
}

void LockContent::refreshBackground(SessionBaseModel::ModeStatus status)
{
    Q_UNUSED(status)

    // 根据当前状态刷新不同的背景
    auto user = m_model->currentUser();
    if (user != nullptr) {
        emit requestBackground(user->greeterBackground());
    }
}

void LockContent::refreshLayout(SessionBaseModel::ModeStatus status)
{
    setTopFrameVisible(status != SessionBaseModel::ModeStatus::ShutDownMode);
    setBottomFrameVisible(status != SessionBaseModel::ModeStatus::ShutDownMode);
}

void LockContent::showTrayPopup(QWidget *trayWidget, QWidget *contentWidget, const bool callShowForce)
{
    if (!trayWidget || !contentWidget) {
        return;
    }

    if (!m_popWin) {
        m_popWin = new PopupWindow(this);
        connect(m_model, &SessionBaseModel::visibleChanged, this, [this] (const bool visible) {
            if (!visible) {
                m_popWin->hide();
            }
        });
        connect(m_popWin, &PopupWindow::visibleChanged, this, [this] (bool visible) {
            m_controlWidget->setCanShowMenu(!visible);
            // BUG-134087, tray popup窗口隐藏后，锁屏界面需要重新抓取键盘
            if (!visible && !m_model->isUseWayland() && isVisible() && window()->windowHandle()) {
                qCDebug(DDE_SHELL) << "Grab keyboard after keyboard layout hidden";
                window()->windowHandle()->setKeyboardGrabEnabled(true);
            }
        });

        connect(this, &LockContent::parentChanged, this, [this] {
            if (m_popWin && m_popWin->isVisible()) {
                const QPoint &point = mapFromGlobal(m_currentTray->mapToGlobal(QPoint(m_currentTray->size().width() / 2, 0)));
                m_popWin->show(point);
            }
        });

        // for BUG-256133
        connect(m_model, &SessionBaseModel::onStatusChanged, this, [this] (SessionBaseModel::ModeStatus status) {
            if (status != SessionBaseModel::ModeStatus::PasswordMode) {
                m_popWin->hide();
            }
        });
    }

    m_currentTray = trayWidget;

    // 隐藏后需要removeEventFilter，后期优化
    for (auto child : this->findChildren<QWidget*>()) {
        child->removeEventFilter(this);
        child->installEventFilter(this);
    }

    if (!m_popWin->getContent() || m_popWin->getContent() != contentWidget)
        m_popWin->setContent(contentWidget);

    const QPoint point = mapFromGlobal(m_currentTray->mapToGlobal(QPoint(m_currentTray->size().width() / 2, 0)));
    // 插件图标不显示，窗口不显示
    if (m_currentTray->isVisible()) {
        m_popWin->move(point.x(), point.y());
        callShowForce ? m_popWin->show(point) : m_popWin->toggle(point);
    }
}

void LockContent::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_mfaWidget) {
            m_mfaWidget->autoUnlock();
        }
        break;
    case Qt::Key_Escape:
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
            m_model->setAbortConfirm(false);
            m_model->setPowerAction(SessionBaseModel::PowerAction::None);
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        } else if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
            m_model->setVisible(false);
        } else if (m_model->currentModeState() == SessionBaseModel::ModeStatus::PowerMode) {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        }
        break;
    }
}

void LockContent::showUserList()
{
    if (m_model->userlistVisible()) {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
        QTimer::singleShot(10, this, [=] {
            m_model->setVisible(true);
        });
    } else {
        std::shared_ptr<User> user_ptr = m_model->findUserByName("...");
        if (user_ptr) {
            emit requestSwitchToUser(user_ptr);
        }
    }
}

void LockContent::showLockScreen()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
    m_model->setVisible(true);
}

void LockContent::showShutdown()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ShutDownMode);
    m_model->setVisible(true);
}
