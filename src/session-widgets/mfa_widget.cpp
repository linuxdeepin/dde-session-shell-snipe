// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mfa_widget.h"

#include "auth_face.h"
#include "auth_fingerprint.h"
#include "auth_iris.h"
#include "auth_password.h"
#include "auth_ukey.h"
#include "dlineeditex.h"
#include "keyboardmonitor.h"
#include "sessionbasemodel.h"
#include "useravatar.h"

MFAWidget::MFAWidget(QWidget *parent)
    : AuthWidget(parent)
    , m_index(0)
    , m_mainLayout(new QVBoxLayout(this))
    , m_authLayout(new QVBoxLayout)
{
    setObjectName(QStringLiteral("MFAWidget"));
    setAccessibleName(QStringLiteral("MFAWidget"));

    setGeometry(0, 0, 280, 176);
    setMinimumSize(280, 176);
}

void MFAWidget::initUI()
{
    AuthWidget::initUI();
    m_authLayout->setSpacing(10);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_authLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_authLayout->setMargin(0);
#endif

    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(10, 0, 10, 0);
    SpacerItemBinder::addWidget(m_userAvatar, m_mainLayout, Qt::AlignVCenter, 0);
    SpacerItemBinder::addWidget(m_accountEdit, m_mainLayout, Qt::AlignVCenter, 24);
    SpacerItemBinder::addWidget(m_userNameWidget, m_mainLayout, Qt::AlignVCenter, 24);
    m_mainLayout->addLayout(m_authLayout);
    m_mainLayout->addSpacing(20);
    m_mainLayout->addWidget(m_expiredStateLabel);
    m_mainLayout->addSpacing(10);
    m_mainLayout->addWidget(m_lockButton, 0, Qt::AlignCenter);
}

void MFAWidget::initConnections()
{
    AuthWidget::initConnections();
    connect(m_model, &SessionBaseModel::authTypeChanged, this, &MFAWidget::setAuthType);
    connect(m_model, &SessionBaseModel::authStateChanged, this, &MFAWidget::setAuthState);
}

void MFAWidget::setModel(const SessionBaseModel *model)
{
    AuthWidget::setModel(model);

    initUI();
    initConnections();

    setAuthType(model->getAuthProperty().AuthType);
    setUser(model->currentUser());
}

void MFAWidget::setAuthType(const AuthFlags type)
{
    m_index = 0;
    /* 面容 */
    if (type & AT_Face) {
        initFaceAuth();
        m_index++;
    } else if (m_faceAuth) {
        m_faceAuth->deleteLater();
        m_faceAuth = nullptr;
    }
    /* 虹膜 */
    if (type & AT_Iris) {
        initIrisAuth();
        m_index++;
    } else if (m_irisAuth) {
        m_irisAuth->deleteLater();
        m_irisAuth = nullptr;
    }
    /* 指纹 */
    if (type & AT_Fingerprint) {
        initFingerprintAuth();
        m_index++;
    } else if (m_fingerprintAuth) {
        m_fingerprintAuth->deleteLater();
        m_fingerprintAuth = nullptr;
    }
    /* Ukey */
    if (type & AT_Ukey) {
        initUKeyAuth();
        m_index++;
    } else if (m_ukeyAuth) {
        m_ukeyAuth->deleteLater();
        m_ukeyAuth = nullptr;
    }
    /* 密码 */
    if (type & AT_Password) {
        initPasswdAuth();
        m_index++;
    } else if (m_passwordAuth) {
        m_passwordAuth->deleteLater();
        m_passwordAuth = nullptr;
    }
    /* 账户 */
    if (type == AT_None) {
        if (m_model->currentUser()->isNoPasswordLogin()) {
            m_lockButton->setEnabled(true);
            m_accountEdit->hide();
            m_userNameWidget->show();
        } else {
            m_accountEdit->clear();
            m_accountEdit->show();
            m_userNameWidget->hide();
        }
    } else {
        const bool visible = m_model->isServerModel() && m_model->appType() == Login;
        m_accountEdit->setVisible(visible);
        m_userNameWidget->setVisible(!visible);
    }

    updatePasswordExpiredState();
    updateGeometry();

    if (m_lockButton->isVisible() && m_lockButton->isEnabled()) {
        setFocusProxy(m_lockButton);
    }
    if (m_passwordAuth) {
        setFocusProxy(m_passwordAuth);
    }
    if (m_ukeyAuth) {
        setFocusProxy(m_ukeyAuth);
    }
    if (m_accountEdit->isVisible()) {
        setFocusProxy(m_accountEdit);
    }

    m_widgetList.clear();
    m_widgetList << m_accountEdit << m_ukeyAuth << m_passwordAuth << m_lockButton;

    for (int i = 0; i < m_widgetList.size(); i++) {
        if (m_widgetList[i]) {
            for (int j = i + 1; j < m_widgetList.size(); j++) {
                if (m_widgetList[j]) {
                    setTabOrder(m_widgetList[i]->focusProxy(), m_widgetList[j]->focusProxy());
                    i = j - 1;
                    break;
                }
            }
        }
    }
    setFocus();
}

/**
 * @brief 认证状态
 * @param type    认证类型
 * @param state  认证结果
 * @param message 消息
 */
void MFAWidget::setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message)
{
    switch (type) {
    case AT_Password:
        if (m_passwordAuth) {
            m_passwordAuth->setAuthState(state, message);
        }
        break;
    case AT_Fingerprint:
        if (m_fingerprintAuth) {
            m_fingerprintAuth->setAuthState(state, message);
        }
        break;
    case AT_Ukey:
        if (m_ukeyAuth) {
            m_ukeyAuth->setAuthState(state, message);
        }
        break;
    case AT_Face:
        if (m_faceAuth) {
            m_faceAuth->setAuthState(state, message);
        }
        break;
    case AT_Iris:
        if (m_irisAuth) {
            m_irisAuth->setAuthState(state, message);
        }
        break;
    case AT_All:
        checkAuthResult(type, state);
        break;
    default:
        break;
    }
}

void MFAWidget::autoUnlock()
{
    if (m_model->currentUser()->isNoPasswordLogin()) {
        m_lockButton->click();
    }
}

/**
 * @brief 密码认证
 */
void MFAWidget::initPasswdAuth()
{
    if (m_passwordAuth) {
        m_passwordAuth->reset();
        m_authLayout->insertWidget(m_index, m_passwordAuth);
        return;
    }
    m_passwordAuth = new AuthPassword(this);
    m_passwordAuth->setCurrentUid(m_model->currentUser()->uid());
    m_passwordAuth->setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());
    m_passwordAuth->setPasswordHint(m_user->passwordHint());
    m_passwordAuth->setAuthStatueVisible(true);
    m_passwordAuth->setPasswordLineEditEnabled(m_model->currentUser()->allowToChangePassword() || m_model->appType() != Login);

    m_authLayout->insertWidget(m_index, m_passwordAuth);

    connect(m_passwordAuth, &AuthPassword::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Password);
    });
    connect(m_passwordAuth, &AuthPassword::requestAuthenticate, this, [this] {
        const QString &text = m_passwordAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        m_passwordAuth->setAuthStateStyle(LOGIN_SPINNER);
        m_passwordAuth->setAnimationState(true);
        m_passwordAuth->setLineEditEnabled(false);
        emit sendTokenToAuth(m_model->currentUser()->name(), AT_Password, text);
    });
    connect(m_passwordAuth, &AuthPassword::authFinished, this, [this](const AuthState value) {
        checkAuthResult(AT_Password, value);
    });
    connect(m_lockButton, &QPushButton::clicked, m_passwordAuth, &AuthPassword::requestAuthenticate);
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged, m_passwordAuth, &AuthPassword::setCapsLockVisible);
    connect(m_passwordAuth, &AuthPassword::requestChangeFocus, this, &MFAWidget::updateFocusPosition);
}

/**
 * @brief 指纹认证
 */
void MFAWidget::initFingerprintAuth()
{
    if (m_fingerprintAuth) {
        m_fingerprintAuth->reset();
        m_authLayout->insertWidget(m_index, m_fingerprintAuth);
        return;
    }
    m_fingerprintAuth = new AuthFingerprint(this);
    m_fingerprintAuth->setAuthFactorType(DDESESSIONCC::MultiAuthFactor);
    m_authLayout->insertWidget(m_index, m_fingerprintAuth);

    connect(m_fingerprintAuth, &AuthFingerprint::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Fingerprint);
    });
    connect(m_fingerprintAuth, &AuthFingerprint::authFinished, this, [this](const AuthState value) {
        checkAuthResult(AT_Fingerprint, value);
    });
}

/**
 * @brief Ukey认证
 */
void MFAWidget::initUKeyAuth()
{
    if (m_ukeyAuth) {
        m_ukeyAuth->reset();
        m_authLayout->insertWidget(m_index, m_ukeyAuth);
        return;
    }
    m_ukeyAuth = new AuthUKey(this);
    m_ukeyAuth->setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());
    m_ukeyAuth->setAuthStatueVisible(true);
    m_authLayout->insertWidget(m_index, m_ukeyAuth);

    connect(m_ukeyAuth, &AuthUKey::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Ukey);
    });
    connect(m_ukeyAuth, &AuthUKey::requestAuthenticate, this, [this] {
        const QString &text = m_ukeyAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        m_ukeyAuth->setAuthStateStyle(LOGIN_SPINNER);
        m_ukeyAuth->setAnimationState(true);
        m_ukeyAuth->setLineEditEnabled(false);
        emit sendTokenToAuth(m_model->currentUser()->name(), AT_Ukey, text);
    });
    connect(m_lockButton, &QPushButton::clicked, m_ukeyAuth, &AuthUKey::requestAuthenticate);
    connect(m_ukeyAuth, &AuthUKey::authFinished, this, [this](const AuthState state) {
        checkAuthResult(AT_Ukey, state);
    });
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged, m_ukeyAuth, &AuthUKey::setCapsLockVisible);
    connect(m_ukeyAuth, &AuthUKey::lineEditTextChanged, this, [this](const QString &value) {
        if (m_model->getAuthProperty().PINLen > 0 && value.size() >= m_model->getAuthProperty().PINLen) {
            emit m_ukeyAuth->requestAuthenticate();
        }
    });
    connect(m_ukeyAuth, &AuthUKey::requestChangeFocus, this, &MFAWidget::updateFocusPosition);
}

/**
 * @brief 人脸认证
 */
void MFAWidget::initFaceAuth()
{
    if (m_faceAuth) {
        m_faceAuth->reset();
        m_authLayout->insertWidget(m_index, m_faceAuth);
        return;
    }
    m_faceAuth = new AuthFace(this);
    m_faceAuth->setAuthFactorType(DDESESSIONCC::MultiAuthFactor);
    m_authLayout->insertWidget(m_index, m_faceAuth);

    connect(m_faceAuth, &AuthFace::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Face);
    });
    connect(m_faceAuth, &AuthFace::authFinished, this, [this](const AuthState state) {
        checkAuthResult(AT_Face, state);
    });
}

/**
 * @brief 虹膜认证
 */
void MFAWidget::initIrisAuth()
{
    if (m_irisAuth) {
        m_irisAuth->reset();
        m_authLayout->insertWidget(m_index, m_irisAuth);
        return;
    }
    m_irisAuth = new AuthIris(this);
    m_irisAuth->setAuthFactorType(DDESESSIONCC::MultiAuthFactor);
    m_authLayout->insertWidget(m_index, m_irisAuth);

    connect(m_irisAuth, &AuthIris::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Iris);
    });
    connect(m_irisAuth, &AuthIris::authFinished, this, [this](const AuthState state) {
        checkAuthResult(AT_Iris, state);
    });
}

/**
 * @brief 检查多因子认证结果
 *
 * @param type
 * @param succeed
 */
void MFAWidget::checkAuthResult(const AuthType type, const AuthState state)
{
    if (type == AT_Password && state == AS_Success) {
        if (m_fingerprintAuth && m_fingerprintAuth->authState() == AS_Locked) {
            m_fingerprintAuth->reset();
            emit m_fingerprintAuth->activeAuth(AT_Fingerprint);
        }
        if (m_faceAuth && m_faceAuth->authState() == AS_Locked) {
            m_faceAuth->reset();
            emit m_faceAuth->activeAuth(AT_Face);
        }
        if (m_irisAuth && m_irisAuth->authState() == AS_Locked) {
            m_irisAuth->reset();
            emit m_irisAuth->activeAuth(AT_Iris);
        }
    }

    if (state == AS_Cancel) {
        if (m_ukeyAuth) {
            m_ukeyAuth->setAuthState(AS_Cancel, "Cancel");
        }
        if (m_passwordAuth) {
            m_passwordAuth->setAuthState(AS_Cancel, "Cancel");
        }
        if (m_fingerprintAuth) {
            m_fingerprintAuth->setAuthState(AS_Cancel, "Cancel");
        }
    }
}

void MFAWidget::updateFocusPosition()
{
    AuthModule *module = static_cast<AuthModule *>(sender());
    if (module == m_passwordAuth) {
        if (m_ukeyAuth && m_ukeyAuth->authState() == AS_Prompt) {
            setFocusProxy(m_ukeyAuth);
        } else {
            setFocusProxy(m_lockButton);
        }
    } else {
        if (m_passwordAuth && m_passwordAuth->isEnabled()) {
            setFocusProxy(m_passwordAuth);
        } else {
            setFocusProxy(m_lockButton);
        }
    }
    setFocus();
}

/**
 * @brief 计算需要增加的顶部间隔(加在用户头像的上方)，以实现用户头像到屏幕顶端的距离为屏幕高度的35%
 */
int MFAWidget::getTopSpacing() const
{
    const int calcTopHeight = static_cast<int>(topLevelWidget()->geometry().height() * AUTH_WIDGET_TOP_SPACING_PERCENT);
    // 验证类型数量*高度
    const int authWidgetHeight = m_index * 47 + MIN_AUTH_WIDGET_HEIGHT;

    // 当分辨率过低时，如果仍然保持用户头像到屏幕顶端的距离为屏幕高度的35%，那么验证窗口整体时偏下的。
    // 计算当验证窗口居中时距离屏幕顶端的高度，与屏幕高度*0.35对比取较小值。
    const int centerTop = static_cast<int>((topLevelWidget()->geometry().height() - authWidgetHeight) / 2);
    const int topHeight = qMin(calcTopHeight, centerTop);

    // 需要额外增加的顶部间隔高度 = 屏幕高度*0.35 - 验证窗口高度 - 时间控件高度 - 布局间隔
    const int deltaY = topHeight - calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN);

    return qMax(15, deltaY);
}

void MFAWidget::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, &MFAWidget::updateBlurEffectGeometry);

    AuthWidget::resizeEvent(event);
}
