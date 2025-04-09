// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "auth_passkey.h"
#include "authcommon.h"

#include <QKeyEvent>
#include <QTimer>
#include <QVBoxLayout>

AuthPasskey::AuthPasskey(QWidget *parent)
    : AuthModule(AuthCommon::AT_Passkey, parent)
    , m_aniIndex(-1)
    , m_textLabel(new DLabel(this))
    , m_spinner(new DSpinner(this))
    , m_stretchLayout(new QHBoxLayout())
{
    setObjectName(QStringLiteral("AuthPasskey"));
    setAccessibleName(QStringLiteral("AuthPasskey"));

    initUI();
    initConnections();
}

/**
 * @brief 初始化界面
 */
void AuthPasskey::initUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(27, 0, 10, 0);
    mainLayout->setSpacing(0);

    /* 旋转提示 */
    m_spinner->setFixedSize(18, 18);

    /* 文案提示 */
    m_textLabel->setText(tr("Please plug in the security key"));
    m_textLabel->setWordWrap(true);

    /* 旋转提示和文案提示布局 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_stretchLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_stretchLayout->setMargin(0);
#endif
    m_stretchLayout->setSpacing(10);
    m_stretchLayout->addStretch(1);
    m_stretchLayout->addWidget(m_spinner, 0);
    m_stretchLayout->addWidget(m_textLabel, 0);
    m_stretchLayout->addStretch(1);

    mainLayout->addLayout(m_stretchLayout, 1);

    /* 认证状态 */
    m_authStateLabel = new DLabel(this);
    setAuthStateStyle(LOGIN_WAIT);
    mainLayout->addWidget(m_authStateLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    needSpinner(false);
    emit retryButtonVisibleChanged(false);
}

/**
 * @brief 初始化信号连接
 */
void AuthPasskey::initConnections()
{
    AuthModule::initConnections();
}

/**
 * @brief AuthPasskey::reset
 */
void AuthPasskey::reset()
{
    m_state = AuthCommon::AS_Ended;
    needSpinner(false);
    m_textLabel->setText(tr("Please plug in the security key"));
}

/**
 * @brief 设置认证状态
 *
 * @param state
 * @param result
 */
void AuthPasskey::setAuthState(const AuthCommon::AuthState state, const QString &result)
{
    m_state = state;
    switch (state) {
    case AuthCommon::AS_Success:
        needSpinner(false);
        setAnimationState(true);
        m_showPrompt = true;
        m_textLabel->setText(tr("Verification successful"));
        emit authFinished(state);
        emit retryButtonVisibleChanged(false);
        break;
    case AuthCommon::AS_Failure:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_showPrompt = true;
        m_textLabel->setText(tr("Verification failed"));
        emit retryButtonVisibleChanged(false);
        emit authFinished(state);
        break;
    case AuthCommon::AS_Cancel:
        needSpinner(false);
        setAnimationState(false);
        m_showPrompt = true;
        break;
    case AuthCommon::AS_Timeout:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_textLabel->setText(result);
        emit retryButtonVisibleChanged(true);
        break;
    case AuthCommon::AS_Error:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_textLabel->setText(result);
        m_showPrompt = true;
        break;
    case AuthCommon::AS_Verify:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_textLabel->setText(result);
        emit retryButtonVisibleChanged(false);
        break;
    case AuthCommon::AS_Exception:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_textLabel->setText(result);
        m_showPrompt = true;
        break;
    case AuthCommon::AS_Prompt:
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        break;
    case AuthCommon::AS_Started:
        needSpinner(true);
        m_textLabel->setText(tr("Identifying the security key"));
        break;
    case AuthCommon::AS_Ended:
        needSpinner(false);
        emit retryButtonVisibleChanged(false);
        break;
    case AuthCommon::AS_Locked:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_showPrompt = false;
        emit retryButtonVisibleChanged(false);
        break;
    case AuthCommon::AS_Recover:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_showPrompt = true;
        break;
    case AuthCommon::AS_Unlocked:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_showPrompt = true;
        emit activeAuth(AuthCommon::AT_Passkey);
        emit retryButtonVisibleChanged(false);
        break;
    default:
        needSpinner(false);
        setAnimationState(false);
        setAuthStateStyle(AUTH_LOCK);
        m_textLabel->setText(result);
        emit retryButtonVisibleChanged(false);
        qCWarning(DDE_SHELL) << "The state of passkey auth is wrong, state: " << state << ", result: " << result;
        break;
    }
    update();
}

/**
 * @brief 设置认证动画状态
 *
 * @param start
 */
void AuthPasskey::setAnimationState(const bool start)
{
    m_aniIndex = 1;

    if (start) {
        m_aniTimer->start(20);
    } else {
        m_aniTimer->stop();
    }
}

/**
 * @brief 更新认证受限信息
 *
 * @param info
 */
void AuthPasskey::setLimitsInfo(const LimitsInfo &info)
{
    AuthModule::setLimitsInfo(info);
}

void AuthPasskey::setAuthFactorType(AuthFactorType authFactorType)
{
    if (DDESESSIONCC::SingleAuthFactor == authFactorType)
        layout()->setContentsMargins(10, 0, 10, 0);

    AuthModule::setAuthFactorType(authFactorType);
}

void AuthPasskey::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit activeAuth(m_type);
    }
    QWidget::mouseReleaseEvent(event);
}

/**
 * @brief 执行动画（认证成功，认证失败，认证中...）
 */
void AuthPasskey::doAnimation()
{
    if (m_state == AuthCommon::AS_Success) {
        if (m_aniIndex > 10) {
            m_aniTimer->stop();
            emit authFinished(AuthCommon::AS_Success);
        } else {
            setAuthStateStyle(QStringLiteral(":/misc/images/unlock/unlock_%1.svg").arg(m_aniIndex++));
        }
    }
}

void AuthPasskey::needSpinner(bool need)
{
    if (need) {
        m_spinner->setVisible(true);
        m_spinner->start();
        m_stretchLayout->setSpacing(10);
    } else {
        m_spinner->setVisible(false);
        m_spinner->stop();
        m_stretchLayout->setSpacing(0);
    }
}
