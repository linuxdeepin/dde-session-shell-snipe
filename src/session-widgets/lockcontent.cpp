#include "lockcontent.h"

#include "src/widgets/controlwidget.h"
#include "sessionbasemodel.h"
#include "userframe.h"
#include "src/widgets/shutdownwidget.h"
#include "src/widgets/virtualkbinstance.h"
#include "src/widgets/logowidget.h"
#include "src/widgets/timewidget.h"
#include "userlogininfo.h"
#include "userloginwidget.h"
#include "userexpiredwidget.h"
#include "userframelist.h"

#include <QMouseEvent>
#include <DDBusSender>

LockContent::LockContent(SessionBaseModel *const model, QWidget *parent)
    : SessionBaseWindow(parent)
    , m_model(model)
    , m_imageBlurInter(new ImageBlur("com.deepin.daemon.Accounts", "/com/deepin/daemon/ImageBlur", QDBusConnection::systemBus(), this))
    , m_virtualKB(nullptr)
    , m_translator(new QTranslator)
    , m_userLoginInfo(new UserLoginInfo(model))
    , m_sessionManager(new SessionManager("com.deepin.SessionManager", "/com/deepin/SessionManager", QDBusConnection::sessionBus(), this))
    , m_accountsInter(new AccountsInter(ACCOUNT_DBUS_SERVICE, ACCOUNT_DBUS_PATH, QDBusConnection::systemBus(), this))
{
    m_controlWidget = new ControlWidget;
    m_shutdownFrame = new ShutdownWidget;
    m_logoWidget = new LogoWidget;

    m_timeWidget = new TimeWidget();
    m_mediaWidget = nullptr;

    m_shutdownFrame->setModel(model);
    restoreCenterContent();

    setCenterTopWidget(m_timeWidget);
    setLeftBottomWidget(m_logoWidget);
    connect(model, &SessionBaseModel::currentUserChanged, this, [ = ](std::shared_ptr<User> user) {
        if (user.get()) {
            m_logoWidget->updateLocale(user->locale().split(".").first());
        }
    });

    switch (model->currentType()) {
    case SessionBaseModel::AuthType::LockType:
        setMPRISEnable(true);
        break;
    default:
        break;
    }
    setRightBottomWidget(m_controlWidget);

    // init connect
    connect(model, &SessionBaseModel::currentUserChanged, this, &LockContent::onCurrentUserChanged);
    connect(m_controlWidget, &ControlWidget::requestSwitchUser, this, [ = ] {
        // 在用户去切换用户时，我们重新获取远程网络账户的状态，去刷新用户列表
        if (m_accountsInter){
            m_accountsInter->UpdateADDomainUserList();
        }

        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::UserMode) return;
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
    });
    connect(m_controlWidget, &ControlWidget::requestShutdown, this, [ = ] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
    });
    connect(m_controlWidget, &ControlWidget::requestSwitchVirtualKB, this, &LockContent::toggleVirtualKB);

    //lixin
    //connect(m_userLoginInfo, &UserLoginInfo::requestAuthUser, this, &LockContent::restoreMode);
    connect(m_userLoginInfo, &UserLoginInfo::requestAuthUser, this, &LockContent::requestAuthUser);
    connect(m_userLoginInfo, &UserLoginInfo::hideUserFrameList, this, &LockContent::restoreCenterContent);
    connect(m_userLoginInfo, &UserLoginInfo::requestSwitchUser, this, &LockContent::requestSwitchToUser);
    connect(m_userLoginInfo, &UserLoginInfo::requestSetLayout, this, &LockContent::requestSetLayout);
    connect(m_userLoginInfo, &UserLoginInfo::changePasswordFinished, this, &LockContent::restoreMode);
    connect(m_userLoginInfo, &UserLoginInfo::unlockActionFinish, this, [&] {
        emit unlockActionFinish();
    });
    connect(m_shutdownFrame, &ShutdownWidget::abortOperation, this, [ = ] {
        if (m_model->powerAction() != SessionBaseModel::RequireShutdown &&
                m_model->powerAction() != SessionBaseModel::RequireRestart)
            restoreMode();
    });

    if (m_model->currentType() == SessionBaseModel::LockType) {
        // 锁屏，点击关机，需要提示“输入密码以关机”。登录不需要这个提示
        connect(m_shutdownFrame, &ShutdownWidget::abortOperation, m_userLoginInfo, [ = ] {
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
        });
    }

    connect(model, &SessionBaseModel::onStatusChanged, this, &LockContent::onStatusChanged);

    auto initVirtualKB = [&](bool hasvirtualkb) {
        if (hasvirtualkb && !m_virtualKB) {
            connect(&VirtualKBInstance::Instance(), &VirtualKBInstance::initFinished, this, [&] {
                m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
                m_controlWidget->setVirtualKBVisible(true);
            }, Qt::QueuedConnection);
            VirtualKBInstance::Instance().init();
        }
    };

    connect(model, &SessionBaseModel::hasVirtualKBChanged, this, initVirtualKB);
    connect(model, &SessionBaseModel::onUserListChanged, this, &LockContent::onUserListChanged);
    connect(model, &SessionBaseModel::userListLoginedChanged, this, &LockContent::onUserListChanged);
    connect(model, &SessionBaseModel::authFinished, this, &LockContent::restoreMode);
    connect(m_imageBlurInter, &ImageBlur::BlurDone, this, &LockContent::onBlurDone);

    QTimer::singleShot(0, this, [ = ] {
        onCurrentUserChanged(model->currentUser());
        initVirtualKB(model->hasVirtualKB());
        onUserListChanged(model->isServerModel() ? model->logindUser() : model->userList());
    });
}

void LockContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (user.get() == nullptr) return; // if dbus is async

    // set user language
    qApp->removeTranslator(m_translator);
    const QString locale { QLocale::system().name().split(".").first() };
    m_translator->load("/usr/share/dde-session-shell/translations/dde-session-shell_" + QLocale(locale.isEmpty() ? "en_US" : locale).name());
    qApp->installTranslator(m_translator);

    //如果是锁屏就用系统语言，如果是登陆界面就用用户语言
    auto locale2 = qApp->applicationName() == "dde-lock" ? QLocale::system().name() : user->locale();
    m_logoWidget->updateLocale(locale2);
    m_timeWidget->updateLocale(locale2);
    m_shutdownFrame->updateLocale(user);

    for (auto connect : m_currentUserConnects) {
        m_user.get()->disconnect(connect);
    }

    m_currentUserConnects.clear();

    m_user = user;

    m_currentUserConnects << connect(user.get(), &User::greeterBackgroundPathChanged, this, &LockContent::requestBackground, Qt::UniqueConnection)
                          << connect(user.get(), &User::use24HourFormatChanged, this, &LockContent::updateTimeFormat, Qt::UniqueConnection);

    //lixin
    m_userLoginInfo->setUser(user);

    //TODO: refresh blur image
    QTimer::singleShot(0, this, [ = ] {
        updateTimeFormat(user->is24HourFormat());
    });
}

void LockContent::pushUserFrame()
{
    if (m_model->isServerModel())
        m_controlWidget->setUserSwitchEnable(false);
    setCenterContent(m_userLoginInfo->getUserFrameList());
}

void LockContent::pushChangeFrame()
{
    setCenterContent(m_userLoginInfo->getUserExpiredWidget());
}

void LockContent::pushConfirmFrame()
{
    setCenterContent(m_userLoginInfo->getUserLoginWidget());
    m_model->setAbortConfirm(true);
}

void LockContent::pushShutdownFrame()
{
    setCenterContent(m_shutdownFrame);
}

void LockContent::setMPRISEnable(const bool state)
{
    if (m_mediaWidget) {
        m_mediaWidget->setVisible(state);
    } else {
        m_mediaWidget = new MediaWidget;
        m_mediaWidget->initMediaPlayer();
        setCenterBottomWidget(m_mediaWidget);
    }
}

void LockContent::beforeUnlockAction(bool is_finish)
{
    m_userLoginInfo->beforeUnlockAction(is_finish);
}

void LockContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    if (m_model->isServerModel())
        onUserListChanged(m_model->logindUser());
    switch (status) {
    case SessionBaseModel::ModeStatus::PasswordMode:
        restoreCenterContent();
        break;
    case SessionBaseModel::ModeStatus::ChangePasswordMode:
        pushChangeFrame();
        break;
    case SessionBaseModel::ModeStatus::ConfirmPasswordMode:
        pushConfirmFrame();
        break;
    case SessionBaseModel::ModeStatus::UserMode:
        pushUserFrame();
        break;
    case SessionBaseModel::ModeStatus::PowerMode:
        pushShutdownFrame();
        break;
    default:
        break;
    }
}

void LockContent::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode)
        m_model->setAbortConfirm(false);

    // hide keyboardlayout widget
    m_userLoginInfo->hideKBLayout();

    return SessionBaseWindow::mouseReleaseEvent(event);
}

void LockContent::showEvent(QShowEvent *event)
{
    onStatusChanged(m_model->currentModeState());

    tryGrabKeyboard();
    return QFrame::showEvent(event);
}

void LockContent::hideEvent(QHideEvent *event)
{
    return QFrame::hideEvent(event);
}

void LockContent::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, [ = ] {
        if (m_virtualKB && m_virtualKB->isVisible())
        {
            updateVirtualKBPosition();
        }
    });

    return QFrame::resizeEvent(event);
}

void LockContent::restoreCenterContent()
{
    auto current_user = m_model->currentUser();
    if (current_user != nullptr && current_user->isLock()) {
        current_user->onLockTimeOut();
    }

    restoreMode();
    setCenterContent(m_userLoginInfo->getUserLoginWidget());
}

void LockContent::restoreMode()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
}

void LockContent::updateTimeFormat(bool use24)
{
    if (m_user != nullptr) {
        m_timeWidget->updateLocale(m_user->locale());
        m_timeWidget->set24HourFormat(use24);
    }
}

void LockContent::onBlurDone(const QString &source, const QString &blur, bool status)
{
    const QString &sourcePath = QUrl(source).isLocalFile() ? QUrl(source).toLocalFile() : source;

    if (status && m_model->currentUser()->greeterBackgroundPath() == sourcePath)
        emit requestBackground(blur);
}

void LockContent::toggleVirtualKB()
{
    if (!m_virtualKB) {
        VirtualKBInstance::Instance();
        QTimer::singleShot(500, this, [ = ] {
            m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
            qDebug() << "init virtualKB over." << m_virtualKB;
            toggleVirtualKB();
        });
        return;
    }

    m_virtualKB->setParent(this);
    m_virtualKB->raise();
    m_userLoginInfo->getUserLoginWidget()->setPassWordEditFocus();

    updateVirtualKBPosition();
    m_virtualKB->setVisible(!m_virtualKB->isVisible());
}

void LockContent::updateVirtualKBPosition()
{
    const QPoint point = mapToParent(QPoint((width() - m_virtualKB->width()) / 2, height() - m_virtualKB->height() - 50));
    m_virtualKB->move(point);
}

void LockContent::onUserListChanged(QList<std::shared_ptr<User> > list)
{
    const bool allowShowUserSwitchButton = m_model->allowShowUserSwitchButton();
    const bool alwaysShowUserSwitchButton = m_model->alwaysShowUserSwitchButton();
    bool haveLogindUser = true;

    if (m_model->isServerModel() && m_model->currentType() == SessionBaseModel::LightdmType) {
        haveLogindUser = !m_model->logindUser().isEmpty();
    }
    m_controlWidget->setUserSwitchEnable((alwaysShowUserSwitchButton ||
                                          (allowShowUserSwitchButton &&
                                           (list.size() > (m_model->isServerModel() ? 0 : 1)))) &&
                                         haveLogindUser);
}

void LockContent::tryGrabKeyboard()
{
#ifdef QT_DEBUG
    return;
#endif

    if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(true)) {
        m_failures = 0;
        return;
    }

    m_failures++;

    if (m_failures == 15) {
        qDebug() << "Trying grabkeyboard has exceeded the upper limit. dde-lock will quit.";
        m_sessionManager->SetLocked(false);

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

        qApp->quit();
        return;
    }

    QTimer::singleShot(100, this, &LockContent::tryGrabKeyboard);
}

void LockContent::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape: {
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode)
            m_model->setAbortConfirm(false);
        break;
    }
    }
}
