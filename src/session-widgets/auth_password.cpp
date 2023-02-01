// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_password.h"

#include "authcommon.h"
#include "dlineeditex.h"
#include "dstyle.h"

#include "accountsuser_interface.h"

#include <DHiDPIHelper>
#include <DLabel>
#include <DPaletteHelper>
#include <DDialogCloseButton>

#include <QKeyEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QApplication>
#include <QDesktopWidget>
#include <QDBusReply>
#include <QWindow>
#include <QValidator>
#include <QRegExp>

#include <unistd.h>

using namespace AuthCommon;

AuthPassword::AuthPassword(QWidget *parent)
    : AuthModule(AT_Password, parent)
    , m_capsLock(new DLabel(this))
    , m_passwordEdit(new DLineEditEx(this))
    , m_passwordHintBtn(new DIconButton(this))
    , m_resetPasswordMessageVisible(false)
    , m_resetPasswordFloatingMessage(nullptr)
    , m_currentUid(0)
    , m_bindCheckTimer(nullptr)
    , m_passwordHintWidget(nullptr)
    , m_iconButton(nullptr)
{
    setObjectName(QStringLiteral("AuthPassword"));
    setAccessibleName(QStringLiteral("AuthPassword"));

    initUI();
    initConnections();

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_passwordEdit->installEventFilter(this);
    setFocusProxy(m_passwordEdit);
}

/**
 * @brief 初始化界面
 */
void AuthPassword::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_passwordEdit->setClearButtonEnabled(false);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_passwordEdit->setFocusPolicy(Qt::StrongFocus);
    m_passwordEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_passwordEdit->lineEdit()->setValidator(new QRegExpValidator(QRegExp("^[ -~]+$")));

    setLineEditInfo(tr("Password"), PlaceHolderText);

    QHBoxLayout *passwordLayout = new QHBoxLayout(m_passwordEdit->lineEdit());
    passwordLayout->setContentsMargins(5, 0, 10, 0);
    passwordLayout->setSpacing(5);
    /* 大小写状态 */
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(CAPS_LOCK);
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_capsLock->setPixmap(pixmap);
    passwordLayout->addWidget(m_capsLock, 0, Qt::AlignLeft | Qt::AlignVCenter);
    /* 缩放因子 */
    passwordLayout->addStretch(1);

    /*显示密码*/
    m_passwordShowBtn->setAccessibleName(QStringLiteral("PasswordShow"));
    m_passwordShowBtn->setContentsMargins(0,0,0,0);
    m_passwordShowBtn->setFocusPolicy(Qt::NoFocus);
    m_passwordShowBtn->setCursor(Qt::ArrowCursor);
    m_passwordShowBtn->setFlat(true);
    m_passwordShowBtn->setIcon(QIcon(PASSWORD_SHOWN));
    m_passwordShowBtn->setIconSize(QSize(16, 16));
    m_passwordShowBtn->setVisible(true);
    passwordLayout->addWidget(m_passwordShowBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    /* 认证状态 */
    m_authStateLabel = new DLabel(this);
    m_authStateLabel->setVisible(false);
    setAuthStateStyle(LOGIN_WAIT);
    passwordLayout->addWidget(m_authStateLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    /* 密码提示 */
    m_passwordHintBtn->setAccessibleName(QStringLiteral("PasswordHint"));
    m_passwordHintBtn->setContentsMargins(0, 0, 0, 0);
    m_passwordHintBtn->setFocusPolicy(Qt::NoFocus);
    m_passwordHintBtn->setCursor(Qt::ArrowCursor);
    m_passwordHintBtn->setFlat(true);
    m_passwordHintBtn->setIcon(QIcon(PASSWORD_HINT));
    m_passwordHintBtn->setIconSize(QSize(16, 16));
    m_passwordHintBtn->setVisible(false);
    passwordLayout->addWidget(m_passwordHintBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(m_passwordEdit);
}

/**
 * @brief 初始化信号连接
 */
void AuthPassword::initConnections()
{
    AuthModule::initConnections();
    /* 密码提示 */
    connect(m_passwordHintBtn, &DIconButton::clicked, this, &AuthPassword::showPasswordHint);
    /* 密码输入框 */
    connect(m_passwordEdit, &DLineEditEx::focusChanged, this, [this](const bool focus) {
        if (!focus) m_passwordEdit->setAlert(false);
        m_authStateLabel->setVisible(!focus && m_showAuthState);
        updatePasswordTextMargins();
        emit focusChanged(focus);
        if (focus) {
            emit lineEditTextChanged(m_lineEdit->text());
        }
    });
    connect(m_passwordEdit, &DLineEditEx::textChanged, this, [this](const QString &text) {
        m_passwordEdit->hideAlertMessage();
        m_passwordEdit->setAlert(false);
        emit lineEditTextChanged(text);
    });
    connect(m_passwordEdit, &DLineEditEx::returnPressed, this, [ this ] {
        if (!m_passwordEdit->lineEdit()->isReadOnly()) // 避免用户在验证的时候反复点击
            emit requestAuthenticate();
    });

    connect(m_passwordShowBtn, &DSuggestButton::clicked, this, [ this ] {
        if (m_lineEdit->echoMode() == QLineEdit::EchoMode::Password) {
            m_passwordShowBtn->setIcon(QIcon(PASSWORD_HIDE));
            m_lineEdit->lineEdit()->setEchoMode(QLineEdit::Normal);
        } else {
            m_passwordShowBtn->setIcon(QIcon(PASSWORD_SHOWN));
            m_lineEdit->lineEdit()->setEchoMode(QLineEdit::Password);
        }
    });
}

/**
 * @brief AuthPassword::reset
 */
void AuthPassword::reset()
{
    m_passwordEdit->clear();
    m_passwordEdit->setAlert(false);
    m_passwordEdit->hideAlertMessage();
    setLineEditEnabled(true);
    setLineEditInfo(tr("Password"), PlaceHolderText);
}

/**
 * @brief 设置认证状态
 *
 * @param state
 * @param result
 */
void AuthPassword::setAuthState(const int state, const QString &result)
{
    qDebug() << "AuthPassword::setAuthResult:" << state << result << m_currentUid;
    m_state = state;
    switch (state) {
    case AS_Success:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_CHECK);
        m_passwordEdit->setAlert(false);
        m_passwordEdit->clear();
        setLineEditEnabled(false);
        setLineEditInfo(tr("Verification successful"), PlaceHolderText);
        m_showPrompt = true;
        m_passwordEdit->hideAlertMessage();
        emit authFinished(state);
        emit requestChangeFocus();
        break;
    case AS_Failure: {
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        m_passwordEdit->clear();
        setLineEditEnabled(true);
        const int leftTimes = static_cast<int>(m_limitsInfo->maxTries - m_limitsInfo->numFailures);
        if (leftTimes > 1) {
            setLineEditInfo(tr("Verification failed, %n chances left", "", leftTimes), PlaceHolderText);
        } else if (leftTimes == 1) {
            setLineEditInfo(tr("Verification failed, only one chance left"), PlaceHolderText);
        }
        setLineEditInfo(tr("Wrong Password"), AlertText);
        m_showPrompt = false;
        emit authFinished(state);
        break;
    }
    case AS_Cancel:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        m_showPrompt = true;
        break;
    case AS_Timeout:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Error:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Verify:
        setAnimationState(true);
        setAuthStateStyle(LOGIN_SPINNER);
        break;
    case AS_Exception:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Prompt:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        if (m_showPrompt) {
            setLineEditInfo(tr("Password"), PlaceHolderText);
        }
        break;
    case AS_Started:
        break;
    case AS_Ended:
        m_passwordEdit->clear();
        break;
    case AS_Locked:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_LOCK);
        setLineEditEnabled(false);
        m_passwordEdit->setAlert(false);
        m_passwordEdit->hideAlertMessage();
        if (m_integerMinutes == 1) {
            setLineEditInfo(tr("Please try again 1 minute later"), PlaceHolderText);
        } else {
            setLineEditInfo(tr("Please try again %n minutes later", "", static_cast<int>(m_integerMinutes)), PlaceHolderText);
        }
        m_showPrompt = false;
        m_passwordHintBtn->hide();
        break;
    case AS_Recover:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        break;
    case AS_Unlocked:
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        m_showPrompt = true;
        break;
    default:
        setAnimationState(false);
        setAuthStateStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        setLineEditInfo(result, AlertText);
        qWarning() << "Error! The state of Password Auth is wrong!" << state << result;
        break;
    }
    update();
}

/**
 * @brief 设置认证动画状态
 *
 * @param start
 */
void AuthPassword::setAnimationState(const bool start)
{
    start ? m_passwordEdit->startAnimation() : m_passwordEdit->stopAnimation();
}

/**
 * @brief 设置大小写图标状态
 *
 * @param on
 */
void AuthPassword::setCapsLockVisible(const bool on)
{
    m_capsLock->setVisible(on);
    updatePasswordTextMargins();
}

/**
 * @brief 更新认证受限信息
 *
 * @param info
 */
void AuthPassword::setLimitsInfo(const LimitsInfo &info)
{
    qDebug() << "AuthPassword::setLimitsInfo" << info.numFailures << m_limitsInfo->locked;
    const bool lockStateChanged = (info.locked != m_limitsInfo->locked);
    AuthModule::setLimitsInfo(info);
    // 如果lock状态发生变化且当前状态为非lock更新编辑框文案
    if (lockStateChanged && !info.locked)
        updateUnlockPrompt();

    m_passwordHintBtn->setVisible(info.numFailures > 0 && !m_passwordHint.isEmpty());
    updatePasswordTextMargins();
    if (m_limitsInfo->numFailures >= 3) {
        if (m_limitsInfo->locked) {
            setAuthState(AS_Locked, "Locked");
        }

        if (this->isVisible() && isShowResetPasswordMessage()) {
            qDebug() << "begin reset passoword";
            setResetPasswordMessageVisible(true);
            updateResetPasswordUI();
        }
    } else if (info.numFailures >= 3) {
        if (this->isVisible() && QFile::exists(ResetPassword_Exe_Path) && m_currentUid <= 9999) {
            qDebug() << "begin reset passoword";
            setResetPasswordMessageVisible(true);
            updateResetPasswordUI();
        }
    } else {
        setResetPasswordMessageVisible(false);
        updateResetPasswordUI();
    }

    if (lockStateChanged)
        emit notifyLockedStateChanged(m_limitsInfo->locked);
}

/**
 * @brief 设置输入框中的文案
 *
 * @param text
 * @param type
 */
void AuthPassword::setLineEditInfo(const QString &text, const TextType type)
{
    switch (type) {
    case AlertText:
        m_passwordEdit->showAlertMessage(text, this, 5000);
        m_passwordEdit->setAlert(true);
        break;
    case InputText: {
        const int cursorPos = m_passwordEdit->lineEdit()->cursorPosition();
        m_passwordEdit->setText(text);
        m_passwordEdit->lineEdit()->setCursorPosition(cursorPos);
        break;
    }
    case PlaceHolderText:
        m_passwordEdit->setPlaceholderText(text);
        break;
    }
}

/**
 * @brief 密码提示
 * @param hint
 */
void AuthPassword::setPasswordHint(const QString &hint)
{
    if (hint == m_passwordHint) {
        return;
    }
    m_passwordHint = hint;
}

void AuthPassword::setCurrentUid(uid_t uid)
{
    m_currentUid = uid;
}

/**
 * @brief 获取输入框中的文字
 *
 * @return QString
 */
QString AuthPassword::lineEditText() const
{
    return m_passwordEdit->text();
}

/**
 * @brief 设置 LineEdit 是否可输入
 *
 * @param enable
 */
void AuthPassword::setLineEditEnabled(const bool enable)
{
    if (enable && !m_limitsInfo->locked) {
        m_passwordEdit->setFocusPolicy(Qt::StrongFocus);
        m_passwordEdit->setFocus();
        m_passwordEdit->lineEdit()->setReadOnly(false);
    } else {
        m_passwordEdit->setFocusPolicy(Qt::NoFocus);
        m_passwordEdit->clearFocus();
        m_passwordEdit->lineEdit()->setReadOnly(true);
    }
}

void AuthPassword::setPasswordLineEditEnabled(const bool enable)
{
    m_passwordLineEditEnabled = enable;
    setLineEditEnabled(enable);
}

/**
 * @brief 更新认证锁定时的文案
 */
void AuthPassword::updateUnlockPrompt()
{
    AuthModule::updateUnlockPrompt();
    if (m_integerMinutes == 1) {
        m_passwordEdit->setPlaceholderText(tr("Please try again 1 minute later"));
    } else if (m_integerMinutes > 1) {
        m_passwordEdit->setPlaceholderText(tr("Please try again %n minutes later", "", static_cast<int>(m_integerMinutes)));
    } else {
        QTimer::singleShot(1000, this, [this] {
            emit activeAuth(m_type);
        });
        qInfo() << "Waiting authentication service...";
    }
    update();
}

/**
 * @brief 显示密码提示
 */
void AuthPassword::showPasswordHint()
{
    m_passwordEdit->showAlertMessage(m_passwordHint, this, 5000);
}

/**
 * @brief 设置密码提示按钮的可见性
 * @param visible
 */
void AuthPassword::setPasswordHintBtnVisible(const bool isVisible)
{
    m_passwordHintBtn->setVisible(isVisible);
    updatePasswordTextMargins();
}

/**
 * @brief 设置重置密码消息框的显示状态数据
 *
 * @param visible
 */
void AuthPassword::setResetPasswordMessageVisible(const bool isVisible)
{
    qDebug() << "set reset password message visible: " << isVisible;
    if (m_resetPasswordMessageVisible == isVisible)
        return;

    m_resetPasswordMessageVisible = isVisible;
    emit resetPasswordMessageVisibleChanged(m_resetPasswordMessageVisible);
}

/**
 * @brief 显示重置密码消息框
 */
void AuthPassword::showResetPasswordMessage()
{
    if (m_resetPasswordFloatingMessage) {
        m_resetPasswordFloatingMessage->show();
        return;
    }

    QWidget *userLoginWidget = parentWidget();
    if (!userLoginWidget) {
        return;
    }

    QWidget *centerFrame = userLoginWidget->parentWidget();
    if (!centerFrame) {
        return;
    }

    QPalette pa;
    pa.setColor(QPalette::Background, QColor(247, 247, 247, 51));
    pa.setColor(QPalette::Highlight, Qt::white);
    pa.setColor(QPalette::HighlightedText, Qt::black);
    m_resetPasswordFloatingMessage = new DFloatingMessage(DFloatingMessage::MessageType::ResidentType);
    m_resetPasswordFloatingMessage->setPalette(pa);
    // DFloatingMessage 中未放开seticonsize接口，无法设置图标大小，使用缩放函数会造成图标锯齿
    // 只能使用findChildren找到对应的图标控件来设置图标大小进行规避
    // DFloatingMessage中有两个按钮一个是DIconButton,另一个是继承于DIconButton的DDialogCloseButton，需要区分
    QList<DIconButton *> btnList = m_resetPasswordFloatingMessage->findChildren<DIconButton *>();
    foreach (const auto iconButton, btnList) {
        DDialogCloseButton * closeButton = qobject_cast<DDialogCloseButton *>(iconButton);
        if (closeButton) {
            continue;
        }
        iconButton->installEventFilter(this);
        m_iconButton = iconButton;
    }
    m_resetPasswordFloatingMessage->setIcon(QIcon("://misc/images/dss_warning.svg"));
    DSuggestButton *suggestButton = new DSuggestButton(tr("Reset Password"));
    suggestButton->setAutoDefault(true);
    m_resetPasswordFloatingMessage->setWidget(suggestButton);
    m_resetPasswordFloatingMessage->setMessage(tr("Forgot password?"));
    connect(suggestButton, &QPushButton::clicked, this, [ this ] {
        const QString AccountsService("org.deepin.dde.Accounts1");
        const QString path = QString("/org/deepin/dde/Accounts1/User%1").arg(m_currentUid);
        org::deepin::dde::accounts1::User user(AccountsService, path, QDBusConnection::systemBus());
        auto reply = user.SetPassword("");
        reply.waitForFinished();
        if (reply.isError())
            qWarning() << "reply setpassword:" << reply.error().message();

        emit m_resetPasswordFloatingMessage->closeButtonClicked();
    });
    connect(m_resetPasswordFloatingMessage, &DFloatingMessage::closeButtonClicked, this, [this](){
        if (m_resetPasswordFloatingMessage) {
            delete  m_resetPasswordFloatingMessage;
            m_resetPasswordFloatingMessage = nullptr;
        }
        emit resetPasswordMessageVisibleChanged(false);
    });
    DMessageManager::instance()->sendMessage(centerFrame, m_resetPasswordFloatingMessage);
}

/**
 * @brief 关闭重置密码消息框
 */
void AuthPassword::closeResetPasswordMessage()
{
    if (m_resetPasswordFloatingMessage) {
        m_resetPasswordFloatingMessage->close();
        delete  m_resetPasswordFloatingMessage;
        m_resetPasswordFloatingMessage = nullptr;
    }
}

/**
 * @brief 当前账户是否绑定unionid
 */
bool AuthPassword::isUserAccountBinded()
{
    QDBusInterface syncHelperInter("com.deepin.sync.Helper",
                                   "/com/deepin/sync/Helper",
                                   "com.deepin.sync.Helper",
                                   QDBusConnection::systemBus());
    QDBusReply<QString> retUOSID = syncHelperInter.call("UOSID");
    if (!syncHelperInter.isValid()) {
        return false;
    }
    QString uosid;
    if (retUOSID.isValid()) {
        uosid = retUOSID.value();
    } else {
        qWarning() << retUOSID.error().message();
        return false;
    }

    QDBusInterface accountsInter("org.deepin.dde.Accounts1",
                                 QString("/org/deepin/dde/Accounts1/User%1").arg(m_currentUid),
                                 "org.deepin.dde.Accounts1.User",
                                 QDBusConnection::systemBus());
    QVariant retUUID = accountsInter.property("UUID");
    if (!accountsInter.isValid()) {
        return false;
    }
    QString uuid = retUUID.toString();

    QDBusReply<QString> retLocalBindCheck= syncHelperInter.call("LocalBindCheck", uosid, uuid);
    if (!syncHelperInter.isValid()) {
        return false;
    }
    QString ubid;
    if (retLocalBindCheck.isValid()) {
        ubid = retLocalBindCheck.value();
        if (m_bindCheckTimer) {
            m_bindCheckTimer->stop();
        }
    } else {
        qWarning() << "UOSID:" << uosid << "uuid:" << uuid;
        qWarning() << retLocalBindCheck.error().message();
        if (retLocalBindCheck.error().message().contains("network error")) {
            if (!m_bindCheckTimer) {
                m_bindCheckTimer = new QTimer(this);
                connect(m_bindCheckTimer, &QTimer::timeout, this, [this] {
                    qWarning() << "BindCheck retry!";
                    if(isUserAccountBinded()) {
                        setResetPasswordMessageVisible(true);
                        updateResetPasswordUI();
                    }
                });
            }
            if (!m_bindCheckTimer->isActive()) {
                m_bindCheckTimer->start(1000);
            }
        }
        return false;
    }

    return !ubid.isEmpty();
}

/**
 * @brief 更新重置密码UI相关状态
 */
void AuthPassword::updateResetPasswordUI()
{
    // >=10000 域管账户, 该功能屏蔽域管账户
    if (m_currentUid > 9999) {
        return;
    }

    if (m_resetPasswordMessageVisible) {
        showResetPasswordMessage();
    } else {
        closeResetPasswordMessage();
    }
}

bool AuthPassword::isShowResetPasswordMessage()
{
    return QFile::exists(DEEPIN_DEEPINID_DAEMON_PATH) && QFile::exists(ResetPassword_Exe_Path) && m_currentUid <= 9999 && !IsCommunitySystem;
}

bool AuthPassword::eventFilter(QObject *watched, QEvent *event)
{
    if (qobject_cast<DLineEditEx *>(watched) == m_passwordEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Cut)
            || keyEvent->matches(QKeySequence::Copy)
            || keyEvent->matches(QKeySequence::Paste)) {
            return true;
        }
    }

    if (watched == m_iconButton && event->type() == QEvent::Paint) {
        QPainter painter(m_iconButton);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        if (!m_iconButton->icon().isNull()) {
            QRect iconRect(0, 0, 20, 20);
            iconRect.moveCenter(m_iconButton->rect().center());
            m_iconButton->icon().paint(&painter, iconRect);
        }

        return true;
    }

    return false;
}

void AuthPassword::hideEvent(QHideEvent *event)
{
    m_passwordEdit->setAlert(false);
    m_passwordEdit->hideAlertMessage();
    setLineEditInfo(tr("Password"), PlaceHolderText);
    closeResetPasswordMessage();
    AuthModule::hideEvent(event);
}

void AuthPassword::showEvent(QShowEvent *event)
{
    m_passwordHintBtn->setVisible(m_limitsInfo->numFailures > 0 && !m_passwordHint.isEmpty());
    updatePasswordTextMargins();
    if (m_limitsInfo->numFailures >= 3) {
        if (m_limitsInfo->locked) {
            setAuthState(AS_Locked, "Locked");
        }

        if (isShowResetPasswordMessage()) {
            qDebug() << "begin reset passoword";
            setResetPasswordMessageVisible(true);
            updateResetPasswordUI();
        }
    } else {
        setResetPasswordMessageVisible(false);
        updateResetPasswordUI();
    }

    AuthModule::showEvent(event);
}

void AuthPassword::setAuthStatueVisible(bool visible)
{
    m_showAuthState = visible;
    m_authStateLabel->setVisible(visible && !hasFocus());
    updatePasswordTextMargins();
}

void AuthPassword::showAlertMessage(const QString &text)
{
    hidePasswordHintWidget();
    m_lineEdit->showAlertMessage(text, this, 5000);
}

void AuthPassword::hidePasswordHintWidget()
{
    if (m_passwordHintWidget) {
        m_passwordHintWidget->deleteLater();
        m_passwordHintWidget = nullptr;
    }

    // 恢复调色板
    DPaletteHelper::instance()->resetPalette(this->topLevelWidget());
}

void AuthPassword::updatePasswordTextMargins()
{
    // 根据大小写提示是否显示，设置密码框左边距,根据密码提示和显示密码设置密码杠右边距
    QMargins textMargins = m_lineEdit->lineEdit()->textMargins();
    textMargins.setLeft(m_capsLock->isVisible() ? m_capsLock->width() : 0);
    textMargins.setRight((m_passwordShowBtn->isVisible() ? m_passwordShowBtn->width() : 0) +
                         (m_authStateLabel->isVisible() ? m_authStateLabel->width() : 0) +
                         (m_passwordHintBtn->isVisible() ? m_passwordHintBtn->width() : 0));
    m_lineEdit->lineEdit()->setTextMargins(textMargins);
}

