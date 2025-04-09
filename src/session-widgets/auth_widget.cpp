// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_widget.h"

#include "auth_face.h"
#include "auth_fingerprint.h"
#include "auth_iris.h"
#include "auth_passkey.h"
#include "auth_password.h"
#include "auth_single.h"
#include "auth_ukey.h"
#include "auth_custom.h"
#include "dlineeditex.h"
#include "keyboardmonitor.h"
#include "sessionbasemodel.h"
#include "useravatar.h"
#include "dconfig_helper.h"

#include <DFontSizeManager>

const int BlurRadius = 15;
const int BlurTransparency = 70;

const QColor ShutdownColor(QColor(247, 68, 68));

AuthWidget::AuthWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_blurEffectWidget(new DBlurEffectWidget(this))
    , m_lockButton(new TransparentButton(this))
    , m_userAvatar(nullptr)
    , m_expiredStateLabel(new DLabel(this))
    , m_accountEdit(nullptr)
    , m_userNameWidget(nullptr)
    , m_singleAuth(nullptr)
    , m_passwordAuth(nullptr)
    , m_fingerprintAuth(nullptr)
    , m_ukeyAuth(nullptr)
    , m_faceAuth(nullptr)
    , m_irisAuth(nullptr)
    , m_passkeyAuth(nullptr)
    , m_customAuth(nullptr)
    , m_refreshTimer(new QTimer(this))
    , m_authState(AuthCommon::AS_None)
{
    setObjectName(QStringLiteral("AuthWidget"));
    setAccessibleName(QStringLiteral("AuthWidget"));

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);

    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();
    connect(m_refreshTimer, &QTimer::timeout, this, [ this ] {
        if(QTime::currentTime().minute() == 0 && QTime::currentTime().second() == 0){
            m_user->updatePasswordExpiredInfo();
            updatePasswordExpiredState();
        }
    });
}

AuthWidget::~AuthWidget()
{

}

void AuthWidget::initUI()
{
    /* 头像 */
    m_userAvatar = new UserAvatar(this);
    m_userAvatar->setFocusPolicy(Qt::NoFocus);
    m_userAvatar->setIcon(m_user->avatar());
    m_userAvatar->setAvatarSize(UserAvatar::AvatarLargeSize);
    /* 用户名输入框 */
    m_accountEdit = new DLineEditEx(this);
    m_accountEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_accountEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_accountEdit->setClearButtonEnabled(false);
    m_accountEdit->setPlaceholderText(tr("Account"));

    // 账户名有效字符使用dsg配置
    const QString accountExpression = DConfigHelper::instance()->getConfig("accountExpression", "[a-zA-Z0-9-_@]+$").toString();
    if (!accountExpression.isEmpty()) {
#ifndef ENABLE_DSS_SNIPE
        m_accountEdit->lineEdit()->setValidator(new QRegExpValidator(QRegExp(accountExpression), this));
#else
        m_accountEdit->lineEdit()->setValidator(new QRegularExpressionValidator(QRegularExpression(accountExpression), this));
#endif
    }
    // 用户名
    m_userNameWidget = new UserNameWidget(false, true, this);
    /* 密码过期提示 */
    m_expiredStateLabel->setAccessibleName("ExpiredStateLabel");
    m_expiredStateLabel->setWordWrap(true);
    m_expiredStateLabel->setAlignment(Qt::AlignHCenter);
    if (!m_model->terminalLocked()) {
        m_expiredStateLabel->hide();
    }
    /* 解锁按钮 */
    if (m_model->appType() == Lock) {
        m_lockButton->setIcon(DStyle::SP_LockElement);
    } else {
        m_lockButton->setIcon(DStyle::SP_ArrowNext);
    }
    m_lockButton->setAccessibleName("LockButton");
    /* 模糊背景 */
    m_blurEffectWidget->setMaskColor(DBlurEffectWidget::LightColor);
    m_blurEffectWidget->setMaskAlpha(BlurTransparency);
    m_blurEffectWidget->setBlurRectXRadius(BlurRadius);
    m_blurEffectWidget->setBlurRectYRadius(BlurRadius);
}

void AuthWidget::initConnections()
{
    /* model */
    connect(m_model, &SessionBaseModel::authFailedTipsMessage, this, &AuthWidget::setAccountErrorMsg);
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &AuthWidget::setUser);
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, [this](SessionBaseModel::PowerAction action) {
        setLockButtonType(action);
    });
    connect(m_model, &SessionBaseModel::checkAccountFinished, this, [this] {
        m_accountEdit->clear();
    });

    /* 用户名 */
    connect(qGuiApp, &QGuiApplication::fontChanged, this, &AuthWidget::updateUserDisplayNameLabel);
    /* 用户名输入框 */
    connect(m_accountEdit, &DLineEditEx::returnPressed, this, [this] {
        if (m_accountEdit->isVisible() && !m_accountEdit->text().isEmpty()) {
            emit requestCheckSameNameAccount(m_accountEdit->text());
        }
    });
    /* 解锁按钮 */
    connect(m_lockButton, &DFloatingButton::clicked, this, [this] {
        // 账户锁定后密码是无效的，这时开启快速登录会直接进入桌面，这里需要排除密码无效的情况
        if (m_user->isNoPasswordLogin() && m_user->isPasswordValid()) {
            emit requestCheckAccount(m_user->name());
        } else if (!m_passwordAuth && !m_ukeyAuth && !m_singleAuth) {
            emit m_accountEdit->returnPressed();
        }
    });
}

void AuthWidget::setModel(const SessionBaseModel *model)
{
    m_model = model;
    m_user = model->currentUser();

    terminalLockedChanged(m_model->terminalLocked());
    connect(m_model, &SessionBaseModel::terminalLockedChanged, this, &AuthWidget::terminalLockedChanged);
}

/**
 * @brief 设置当前用户
 * @param user
 */
void AuthWidget::setUser(std::shared_ptr<User> user)
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connectionList)) {
        m_user->disconnect(connection);
    }
    m_connectionList.clear();
    m_user = user;
    m_connectionList << connect(user.get(), &User::avatarChanged, this, &AuthWidget::setAvatar)
                     << connect(user.get(), &User::displayNameChanged, this, &AuthWidget::updateUserDisplayNameLabel)
                     << connect(user.get(), &User::passwordHintChanged, this, &AuthWidget::setPasswordHint)
                     << connect(user.get(), &User::limitsInfoChanged, this, &AuthWidget::setLimitsInfo)
                     << connect(user.get(), &User::passwordExpiredInfoChanged, this, &AuthWidget::updatePasswordExpiredState)
                     << connect(user.get(), &User::noPasswordLoginChanged, this, &AuthWidget::onNoPasswordLoginChanged);

    setAvatar(user->avatar());
    setPasswordHint(user->passwordHint());
    setLimitsInfo(user->limitsInfo());
    if (!m_model->terminalLocked()) {
        updatePasswordExpiredState();
    }
    updateUserDisplayNameLabel();

    /* 根据用户类型设置用户名和用户名输入框的可见性 */
    if (user->type() == User::Default) {
        m_userNameWidget->hide();
        m_accountEdit->show();
        this->setFocusProxy(m_accountEdit);
        m_accountEdit->setFocus();
    } else {
        m_userNameWidget->show();
        m_accountEdit->hide();
        m_accountEdit->clear();
    }

    if (m_singleAuth) {
        m_singleAuth->setCurrentUid(user->uid());
    }
    if (m_passwordAuth) {
        m_passwordAuth->setCurrentUid(user->uid());
    }

    // A账户为无密码登录，B账户有密码；在登录界面的时候，B账户输入密码不登录，切换到A，再切换到B，发现B的密码输入框还有密码。
    if (m_passwordAuth) {
        m_passwordAuth->reset();
    }
}

/**
 * @brief 设置认证类型
 * @param type
 */
void AuthWidget::setAuthType(const AuthCommon::AuthFlags type)
{
    Q_UNUSED(type)
}

/**
 * @brief 设置认证状态
 * @param type
 * @param state
 * @param message
 */
void AuthWidget::setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message)
{
    Q_UNUSED(type)
    Q_UNUSED(state)
    Q_UNUSED(message)

    m_authState = static_cast<AuthCommon::AuthState>(state);
}

/**
 * @brief AuthWidget::checkAuthResult
 * @param type
 * @param state
 */
void AuthWidget::checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state)
{
    Q_UNUSED(type)
    Q_UNUSED(state)
}

/**
 * @brief 设置认证限制信息
 * @param limitsInfo
 */
void AuthWidget::setLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo)
{
    User::LimitsInfo limitsInfoTmpU;
    LimitsInfo limitsInfoTmp;

    QMap<int, User::LimitsInfo>::const_iterator i = limitsInfo->constBegin();
    while (i != limitsInfo->end()) {
        limitsInfoTmpU = i.value();
        limitsInfoTmp.locked = limitsInfoTmpU.locked;
        limitsInfoTmp.maxTries = limitsInfoTmpU.maxTries;
        limitsInfoTmp.numFailures = limitsInfoTmpU.numFailures;
        limitsInfoTmp.unlockSecs = limitsInfoTmpU.unlockSecs;
        limitsInfoTmp.unlockTime = limitsInfoTmpU.unlockTime;
        switch (i.key()) {
        case AT_PAM:
            if (m_singleAuth) {
                m_singleAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Password:
            if (m_passwordAuth) {
                m_passwordAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Fingerprint:
            if (m_fingerprintAuth) {
                m_fingerprintAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Ukey:
            if (m_ukeyAuth) {
                m_ukeyAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Face:
            if (m_faceAuth) {
                m_faceAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Passkey:
            if (m_passkeyAuth) {
                m_passkeyAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_Iris:
            if (m_irisAuth) {
                m_irisAuth->setLimitsInfo(limitsInfoTmp);
            }
            break;
        case AT_ActiveDirectory:
        case AT_Custom:
            break;
        default:
            qCWarning(DDE_SHELL) << "Authentication type is wrong." << i.key();
            break;
        }
        ++i;
    }

    if (m_customAuth) {
        m_customAuth->setLimitsInfo(*limitsInfo);
    }
}

/**
 * @brief 设置用户头像
 * @param avatar
 */
void AuthWidget::setAvatar(const QString &avatar)
{
    m_userAvatar->setIcon(avatar);
}

/**
 * @brief 设置用户名字体
 * @param font
 */
void AuthWidget::updateUserDisplayNameLabel()
{
    m_userNameWidget->updateFullName(m_user->fullName());
    m_userNameWidget->updateUserName(m_user->name());
    m_userNameWidget->setDomainUserVisible(m_user->isDomainUser());
}

/**
 * @brief 设置密码提示
 * @param hint
 */
void AuthWidget::setPasswordHint(const QString &hint)
{
    if (m_singleAuth) {
        m_singleAuth->setPasswordHint(hint);
    }
    if (m_passwordAuth) {
        m_passwordAuth->setPasswordHint(hint);
    }
}

void AuthWidget::onNoPasswordLoginChanged(bool noPassword)
{
    if (noPassword)
        setAuthType(AT_None);

    Q_EMIT noPasswordLoginChanged(m_user->name(), noPassword);
}

/**
 * @brief 设置解锁按钮的样式
 * @param type
 */
void AuthWidget::setLockButtonType(const int type)
{
    QPalette lockPalette;
    switch (type) {
    case SessionBaseModel::RequireRestart:
        m_lockButton->setIcon(QIcon(":/img/bottom_actions/reboot.svg"));
        m_lockButton->setColor(ShutdownColor);
        break;
    case SessionBaseModel::RequireShutdown:
        m_lockButton->setIcon(QIcon(":/img/bottom_actions/shutdown.svg"));
        m_lockButton->setColor(ShutdownColor);
        break;
    default:
        if (m_model->appType() == Login) {
            m_lockButton->setIcon(DStyle::SP_ArrowNext);
        } else {
            m_lockButton->setIcon(DStyle::SP_LockElement);
        }
        // 设置为无效值，自动取活动色
        m_lockButton->setColor(QColor());
        break;
    }
}

/**
 * @brief 设置账户错误信息
 * @param message
 */
void AuthWidget::setAccountErrorMsg(const QString &message)
{
    m_accountEdit->showAlertMessage(message);
}

/**
 * @brief 更新模糊背景参数
 */
void AuthWidget::updateBlurEffectGeometry()
{
    QRect rect = this->rect();
    rect.setTop(m_userAvatar->geometry().top() + m_userAvatar->height() / 2);

    if (m_user->expiredState() == User::ExpiredNormal && !m_model->terminalLocked()) {
        rect.setBottom(m_lockButton->geometry().top() - 20);
    } else {
        rect.setBottom(m_expiredStateLabel->geometry().top() - 10);
    }

    m_blurEffectWidget->setGeometry(rect);
}

/**
 * @brief 密码过期提示
 */
void AuthWidget::updatePasswordExpiredState()
{
    // 锁定情况下不更新密码过期信息
    if (m_model->terminalLocked()) {
        return;
    }

    switch (m_user->expiredState()) {
    case User::ExpiredNormal:
        m_expiredStateLabel->clear();
        m_expiredStateLabel->hide();
        break;
    case User::ExpiredSoon:
        m_expiredStateLabel->setText(tr("Your password will expire in %n days, please change it timely", "", m_user->expiredDayLeft()));
        m_expiredStateLabel->show();
        break;
    case User::ExpiredAlready:
        m_expiredStateLabel->setText(m_user->allowToChangePassword() ? tr("Password expired, please change") : tr("Your password has expired. Please contact the administrator to change it"));
        m_expiredStateLabel->show();
        break;
    default:
        break;
    }
    if (m_passwordAuth) {
        m_passwordAuth->setPasswordLineEditEnabled(m_model->appType() != Login || m_user->allowToChangePassword());
    }
    if (m_singleAuth) {
        m_singleAuth->setPasswordLineEditEnabled(m_model->appType() != Login || m_user->allowToChangePassword());
    }
}

/**
 * @brief 将同步数据的方法注册到单例类
 * @param flag
 * @param function
 */
void AuthWidget::registerSyncFunctions(const QString &flag, std::function<void(QVariant)> function)
{

}

/**
 * @brief 单因数据同步
 * @param value
 */
void AuthWidget::syncSingle(const QVariant &value)
{
    if (m_singleAuth)
        m_singleAuth->setLineEditInfo(value.toString(), AuthPassword::InputText);
}

/**
 * @brief 单因重置密码可见性数据同步
 *
 * @param value
 */
void AuthWidget::syncSingleResetPasswordVisibleChanged(const QVariant &value)
{
    if (m_singleAuth) {
        m_singleAuth->setResetPasswordMessageVisible(value.toBool());
        m_singleAuth->updateResetPasswordUI();
    }
}

/**
 * @brief 用户名编辑框数据同步
 * @param value
 */
void AuthWidget::syncAccount(const QVariant &value)
{
    int cursorIndex = m_accountEdit->lineEdit()->cursorPosition();
    m_accountEdit->setText(value.toString());
    m_accountEdit->lineEdit()->setCursorPosition(cursorIndex);
}

/**
 * @brief 密码编辑框数据同步
 * @param value
 */
void AuthWidget::syncPassword(const QVariant &value)
{
    if (m_passwordAuth)
        m_passwordAuth->setLineEditInfo(value.toString(), AuthPassword::InputText);
}

/**
 * @brief 密码重置密码可见性数据同步
 *
 * @param value
 */
void AuthWidget::syncPasswordResetPasswordVisibleChanged(const QVariant &value)
{
    if (m_passwordAuth) {
        m_passwordAuth->setResetPasswordMessageVisible(value.toBool(), true);
        if (m_passwordAuth->isVisible())
            m_passwordAuth->updateResetPasswordUI();
    }
}

/**
 * @brief 密码重置密码可见性UI同步
 */
void AuthWidget::syncResetPasswordUI()
{
    if (m_singleAuth) {
        m_singleAuth->updateResetPasswordUI();
    }
    if (m_passwordAuth && m_passwordAuth->isVisible()) {
        m_passwordAuth->updateResetPasswordUI();
    }
}

/**
 * @brief UKey 编辑框数据同步
 * @param value
 */
void AuthWidget::syncUKey(const QVariant &value)
{
    if (m_passwordAuth)
        m_ukeyAuth->setLineEditInfo(value.toString(), AuthUKey::InputText);
}

void AuthWidget::showEvent(QShowEvent *event)
{
    activateWindow();

    if (focusProxy()) {
        focusProxy()->setFocus();
    }

    QWidget::showEvent(event);
}

int AuthWidget::getTopSpacing() const
{
    const int topHeight = static_cast<int>(topLevelWidget()->geometry().height() * AUTH_WIDGET_TOP_SPACING_PERCENT);
    const int deltaY = topHeight - calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN);

    return qMax(15, deltaY);
}

int AuthWidget::calcCurrentHeight(const int height) const
{
    const int h = static_cast<int>(((double) height / (double) BASE_SCREEN_HEIGHT) * topLevelWidget()->geometry().height());
    return qMin(h, height);
}

void AuthWidget::terminalLockedChanged(bool locked)
{
    qCInfo(DDE_SHELL) << "AuthWidget terminal locked changed:" << locked;
    if (locked) {
        m_expiredStateLabel->setText(tr("Terminal locked, please contact administrator"));
        m_expiredStateLabel->show();
        m_lockButton->setEnabled(false);
    } else {
        m_expiredStateLabel->clear();
        m_expiredStateLabel->hide();
    }
}

void AuthWidget::syncPasswordErrorTipsClearChanged(const QVariant &value)
{
    if (m_passwordAuth ) {
        m_passwordAuth->clearPasswrodErrorTip(value.toBool());
    }
}

void AuthWidget::updatePasswordErrortipUi()
{
    if (m_passwordAuth && m_passwordAuth->canShowPasswrodErrorTip()) {
        m_passwordAuth->updatePasswrodErrorTipUi();
    }
}
