// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_single.h"

#include "authcommon.h"
#include "dlineeditex.h"

#include <DHiDPIHelper>
#include <DDialogCloseButton>

#include <QKeyEvent>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QApplication>
#include <QDesktopWidget>
#include <QDBusReply>
#include <QWindow>
#include <QValidator>
#include <QRegExp>

#include <unistd.h>
#include <com_deepin_daemon_accounts_user.h>

#define Service "com.deepin.dialogs.ResetPassword"
#define Path "/com/deepin/dialogs/ResetPassword"
#define Interface "com.deepin.dialogs.ResetPassword"

using namespace AuthCommon;

AuthSingle::AuthSingle(QWidget *parent)
    : AuthModule(AT_PAM, parent)
    , m_capsLock(new DLabel(this))
    , m_lineEdit(new DLineEditEx(this))
    , m_keyboardBtn(new DPushButton(this))
    , m_passwordHintBtn(new DIconButton(this))
    , m_resetPasswordMessageVisible(false)
    , m_resetPasswordFloatingMessage(nullptr)
    , m_bindCheckTimer(nullptr)
{
    setObjectName(QStringLiteral("AuthSingle"));
    setAccessibleName(QStringLiteral("AuthSingle"));

    initUI();
    initConnections();

    m_lineEdit->installEventFilter(this);
    setFocusProxy(m_lineEdit);
}

/**
 * @brief 初始化界面
 */
void AuthSingle::initUI()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_lineEdit->setClearButtonEnabled(false);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    m_lineEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_lineEdit->lineEdit()->setValidator(new QRegExpValidator(QRegExp("^[ -~]+$"), this));

    auto *passwordLayout = new QHBoxLayout(m_lineEdit->lineEdit());
    passwordLayout->setContentsMargins(0, 0, 10, 0);
    passwordLayout->setSpacing(0);
    /* 键盘布局按钮 */
    m_keyboardBtn->setAccessibleName(QStringLiteral("KeyboardButton"));
    m_keyboardBtn->setContentsMargins(0, 0, 0, 0);
    m_keyboardBtn->setFocusPolicy(Qt::NoFocus);
    m_keyboardBtn->setCursor(Qt::ArrowCursor);
    m_keyboardBtn->setFlat(true);
    passwordLayout->addWidget(m_keyboardBtn, 0, Qt::AlignLeft | Qt::AlignVCenter);
    /* 缩放因子 */
    passwordLayout->addStretch(1);
    /* 大小写状态 */
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/misc/images/caps_lock.svg");
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_capsLock->setAccessibleName(QStringLiteral("CapsStateLabel"));
    m_capsLock->setPixmap(pixmap);
    passwordLayout->addWidget(m_capsLock, 0, Qt::AlignRight | Qt::AlignVCenter);
    /* 密码提示 */
    m_passwordHintBtn->setAccessibleName(QStringLiteral("PasswordHintButton"));
    m_passwordHintBtn->setContentsMargins(0, 0, 0, 0);
    m_passwordHintBtn->setFocusPolicy(Qt::NoFocus);
    m_passwordHintBtn->setCursor(Qt::ArrowCursor);
    m_passwordHintBtn->setFlat(true);
    m_passwordHintBtn->setIcon(QIcon(PASSWORD_HINT));
    m_passwordHintBtn->setIconSize(QSize(16, 16));
    m_passwordHintBtn->setVisible(false);
    passwordLayout->addWidget(m_passwordHintBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(m_lineEdit);
    updatePasswordTextMargins();
}

/**
 * @brief 初始化信号连接
 */
void AuthSingle::initConnections()
{
    AuthModule::initConnections();
    /* 键盘布局按钮 */
    connect(m_keyboardBtn, &QPushButton::clicked, this, &AuthSingle::requestShowKeyboardList);
    /* 密码提示 */
    connect(m_passwordHintBtn, &DIconButton::clicked, this, &AuthSingle::showPasswordHint);
    /* 输入框 */
    connect(m_lineEdit, &DLineEditEx::focusChanged, this, [this](const bool focus) {
        if (!focus) {
            m_lineEdit->setAlert(false);
        }
        emit focusChanged(focus);
    });
    connect(m_lineEdit, &DLineEditEx::textChanged, this, [this](const QString &text) {
        m_lineEdit->setAlert(false);
        m_lineEdit->hideAlertMessage();
        emit lineEditTextChanged(text);
    });
    connect(m_lineEdit, &DLineEditEx::returnPressed, this, [this] {
        if (!m_lineEdit->text().isEmpty() && !m_lineEdit->lineEdit()->isReadOnly()) {
            setAnimationState(true);
            m_lineEdit->clearFocus();
            m_lineEdit->setFocusPolicy(Qt::NoFocus);
            m_lineEdit->lineEdit()->setReadOnly(true);
            emit requestAuthenticate();
        }
    });
}

/**
 * @brief AuthSingle::reset
 */
void AuthSingle::reset()
{
    m_lineEdit->clear();
    m_lineEdit->setAlert(false);
    m_lineEdit->hideAlertMessage();
    setLineEditEnabled(true);
}

/**
 * @brief 设置认证状态
 *
 * @param state
 * @param result
 */
void AuthSingle::setAuthState(const int state, const QString &result)
{
    qDebug() << "AuthSingle::setAuthResult:" << state << result;
    m_state = state;
    switch (state) {
    case AS_Success:
        setAnimationState(false);
        m_lineEdit->setAlert(false);
        m_lineEdit->clear();
        setLineEditEnabled(false);
        setLineEditInfo(result, PlaceHolderText);
        emit authFinished(AS_Success);
        break;
    case AS_Failure: {
        setAnimationState(false);
        m_lineEdit->clear();
        if (m_limitsInfo->locked) {
            m_lineEdit->setAlert(false);
            setLineEditEnabled(false);
            if (m_integerMinutes == 1) {
                setLineEditInfo(tr("Please try again 1 minute later"), PlaceHolderText);
            } else {
                setLineEditInfo(tr("Please try again %n minutes later", "", static_cast<int>(m_integerMinutes)), PlaceHolderText);
            }
        } else {
            setLineEditEnabled(true);
            setLineEditInfo(result, AlertText);
        }
        break;
    }
    case AS_Cancel:
        setAnimationState(false);
        m_lineEdit->setAlert(false);
        m_lineEdit->hideAlertMessage();
        break;
    case AS_Timeout:
        setAnimationState(false);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Error:
        setAnimationState(false);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Verify:
        setAnimationState(true);
        break;
    case AS_Exception:
        setAnimationState(false);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Prompt:
        setAnimationState(false);
        m_lineEdit->clear();
        setLineEditEnabled(true);
        setLineEditInfo(result, PlaceHolderText);
        break;
    case AS_Started:
        break;
    case AS_Ended:
        break;
    case AS_Locked:
        setAnimationState(false);
        break;
    case AS_Recover:
        setAnimationState(false);
        break;
    case AS_Unlocked:
        break;
    default:
        setAnimationState(false);
        setLineEditInfo(result, AlertText);
        qWarning() << "Error! The state of Password Auth is wrong!" << state << result;
        break;
    }
    update();
}

/**
 * @brief 设置大小写图标状态
 *
 * @param on
 */
void AuthSingle::setCapsLockVisible(const bool on)
{
    m_capsLock->setVisible(on);
    updatePasswordTextMargins();
}

/**
 * @brief 设置认证动画状态
 *
 * @param start
 */
void AuthSingle::setAnimationState(const bool start)
{
    start ? m_lineEdit->startAnimation() : m_lineEdit->stopAnimation();
}

/**
 * @brief 更新认证受限信息
 *
 * @param info
 */
void AuthSingle::setLimitsInfo(const LimitsInfo &info)
{
    qDebug() << "AuthSingle::setLimitsInfo" << info.numFailures;
    const bool lockStateChanged = (info.locked != m_limitsInfo->locked);
    AuthModule::setLimitsInfo(info);
    setPasswordHintBtnVisible(info.numFailures > 0 && !m_passwordHint.isEmpty());
    if (m_limitsInfo->numFailures >= 3) {
        bool isShow = lockStateChanged && this->isVisible() && QFile::exists(DEEPIN_DEEPINID_DAEMON_PATH) &&
                QFile::exists(ResetPassword_Exe_Path) && m_currentUid <= 9999 && !IsCommunitySystem;
        if (isShow) {
            qDebug() << "begin reset passoword";
            setResetPasswordMessageVisible(true);
            updateResetPasswordUI();
        }
    } else {
        setResetPasswordMessageVisible(false);
        updateResetPasswordUI();
    }
}

/**
 * @brief 设置 LineEdit 是否可输入
 *
 * @param enable
 */
void AuthSingle::setLineEditEnabled(const bool enable)
{
    // m_lineEdit->setEnabled(enable);
    if (enable) {
        m_lineEdit->setFocusPolicy(Qt::StrongFocus);
        m_lineEdit->setFocus();
        m_lineEdit->lineEdit()->setReadOnly(false);
    } else {
        m_lineEdit->setFocusPolicy(Qt::NoFocus);
        m_lineEdit->clearFocus();
        m_lineEdit->lineEdit()->setReadOnly(true);
    }
}

/**
 * @brief 设置输入框中的文案
 *
 * @param text
 * @param type
 */
void AuthSingle::setLineEditInfo(const QString &text, const TextType type)
{
    switch (type) {
    case AlertText:
        m_lineEdit->showAlertMessage(text, this, 5000);
        m_lineEdit->setAlert(true);
        break;
    case InputText: {
        const int cursorPos = m_lineEdit->lineEdit()->cursorPosition();
        m_lineEdit->setText(text);
        m_lineEdit->lineEdit()->setCursorPosition(cursorPos);
        break;
    }
    case PlaceHolderText:
        m_lineEdit->setPlaceholderText(text);
        break;
    }
}

/**
 * @brief 设置密码提示内容
 * @param hint
 */
void AuthSingle::setPasswordHint(const QString &hint)
{
    if (hint == m_passwordHint) {
        return;
    }
    m_passwordHint = hint;
}

void AuthSingle::setCurrentUid(uid_t uid)
{
    m_currentUid = uid;
}

/**
 * @brief 设置键盘布局按钮显示的文案
 *
 * @param text
 */
void AuthSingle::setKeyboardButtonInfo(const QString &text)
{
    QString currentText = text.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/")) {
        currentText = currentText.split("/").last();
    }
    m_keyboardBtn->setText(currentText);
}

/**
 * @brief 设置键盘布局按钮的显示状态
 *
 * @param visible
 */
void AuthSingle::setKeyboardButtonVisible(const bool visible)
{
    m_keyboardBtn->setVisible(visible);
}

/**
 * @brief 设置重置密码消息框的显示状态数据
 *
 * @param visible
 */
void AuthSingle::setResetPasswordMessageVisible(const bool isVisible)
{
    if (m_resetPasswordMessageVisible == isVisible)
        return;

    // 如果设置为显示重置按钮，失败次数>=3次就显示重置密码:1060-24505
    if (isVisible && m_limitsInfo && m_limitsInfo->numFailures < 3)
        return;

    m_resetPasswordMessageVisible = isVisible;
    emit resetPasswordMessageVisibleChanged(m_resetPasswordMessageVisible);
}

/**
 * @brief 获取输入框中的文字
 *
 * @return QString
 */
QString AuthSingle::lineEditText() const
{
    return m_lineEdit->text();
}

/**
 * @brief 更新认证锁定时的文案
 */
void AuthSingle::updateUnlockPrompt()
{
    qDebug() << "AuthSingle::updateUnlockPrompt:" << m_integerMinutes;
    if (m_integerMinutes == 1) {
        m_lineEdit->setPlaceholderText(tr("Please try again 1 minute later"));
    } else if (m_integerMinutes > 1) {
        m_lineEdit->setPlaceholderText(tr("Please try again %n minutes later", "", static_cast<int>(m_integerMinutes)));
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
void AuthSingle::showPasswordHint()
{
    m_lineEdit->showAlertMessage(m_passwordHint, this, 5000);
}

/**
 * @brief 设置密码提示按钮的可见性
 * @param visible
 */
void AuthSingle::setPasswordHintBtnVisible(const bool isVisible)
{
    m_passwordHintBtn->setVisible(isVisible);
    updatePasswordTextMargins();
}

/**
 * @brief 显示重置密码消息框
 */
void AuthSingle::showResetPasswordMessage()
{
    if (m_resetPasswordFloatingMessage && m_resetPasswordFloatingMessage->isVisible()) {
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
    // DFloatingMessage 中未放开seticonsize接口，无法设置图标大小，使用缩放会造成图标锯齿
    // 只能使用findChildren找到对应的图标控件来设置图标大小进行规避
    // DFloatingMessage中有两个按钮一个是DIconButton,另一个是继承于DIconButton的DDialogCloseButton，需要区分
    QList<DIconButton *> btnList = m_resetPasswordFloatingMessage->findChildren<DIconButton *>();
    foreach (const auto iconButton, btnList) {
        DDialogCloseButton * closeButton = qobject_cast<DDialogCloseButton *>(iconButton);
        if (closeButton) {
            continue;
        }
        iconButton->setIconSize(QSize(20, 20));
    }
    m_resetPasswordFloatingMessage->setIcon(QIcon::fromTheme("gtk-dialog-error"));
    DSuggestButton *suggestButton = new DSuggestButton(tr("Reset Password"));
    suggestButton->setAutoDefault(true);
    m_resetPasswordFloatingMessage->setWidget(suggestButton);
    m_resetPasswordFloatingMessage->setMessage(tr("Forgot password?"));
    connect(suggestButton, &QPushButton::clicked, this, [ this ]{
        const QString AccountsService("com.deepin.daemon.Accounts");
        const QString path = QString("/com/deepin/daemon/Accounts/User%1").arg(m_currentUid);
        com::deepin::daemon::accounts::User user(AccountsService, path, QDBusConnection::systemBus());
        auto reply = user.SetPassword("");
        reply.waitForFinished();
        qWarning() << "reply setpassword:" << reply.error().message();

        emit m_resetPasswordFloatingMessage->closeButtonClicked();
    });
    DMessageManager::instance()->sendMessage(centerFrame, m_resetPasswordFloatingMessage);
    connect(m_resetPasswordFloatingMessage, &DFloatingMessage::closeButtonClicked, this, [this](){
        if (m_resetPasswordFloatingMessage) {
            delete  m_resetPasswordFloatingMessage;
            m_resetPasswordFloatingMessage = nullptr;
        }
        emit resetPasswordMessageVisibleChanged(false);
    });
}

/**
 * @brief 关闭重置密码消息框
 */
void AuthSingle::closeResetPasswordMessage()
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
bool AuthSingle::isUserAccountBinded()
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

    QDBusInterface accountsInter("com.deepin.daemon.Accounts",
                                 QString("/com/deepin/daemon/Accounts/User%1").arg(m_currentUid),
                                 "com.deepin.daemon.Accounts.User",
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
            if (m_bindCheckTimer == nullptr) {
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
    if(!ubid.isEmpty()) {
        return true;
    } else {
        return false;
    }
}

void AuthSingle::updatePasswordTextMargins()
{
    // 根据大小写提示是否显示，设置密码框左边距,根据密码提示和显示密码设置密码杠右边距
    QMargins textMargins = m_lineEdit->lineEdit()->textMargins();
    textMargins.setLeft(m_capsLock->isVisible() ? m_capsLock->width() : 0);
    textMargins.setRight(m_passwordHintBtn->isVisible() ? m_passwordHintBtn->width() : 0);
    m_lineEdit->lineEdit()->setTextMargins(textMargins);
}

/**
 * @brief 更新重置密码UI相关状态
 */
void AuthSingle::updateResetPasswordUI()
{
    if (m_currentUid > 9999) {
        return;
    }

    if (m_resetPasswordMessageVisible) {
        showResetPasswordMessage();
    } else {
        closeResetPasswordMessage();
    }
}

bool AuthSingle::eventFilter(QObject *watched, QEvent *event)
{
    if (qobject_cast<DLineEditEx *>(watched) == m_lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Cut)
            || keyEvent->matches(QKeySequence::Copy)
            || keyEvent->matches(QKeySequence::Paste)) {
            return true;
        }
    }

    if (watched == this && event->type() == QEvent::Hide) {
        closeResetPasswordMessage();
    }

    return false;
}
