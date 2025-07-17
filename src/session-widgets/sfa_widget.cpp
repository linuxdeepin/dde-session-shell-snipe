// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sfa_widget.h"

#include "auth_custom.h"
#include "auth_face.h"
#include "auth_passkey.h"
#include "auth_fingerprint.h"
#include "auth_iris.h"
#include "auth_password.h"
#include "auth_single.h"
#include "auth_ukey.h"
#include "authcommon.h"
#include "dlineeditex.h"
#include "keyboardmonitor.h"
#include "sessionbasemodel.h"
#include "useravatar.h"
#include "plugin_manager.h"
#include "login_plugin_util.h"
#include "signal_bridge.h"

#include <DFontSizeManager>

#include <QSpacerItem>

const QSize AuthButtonSize(60, 36);
const QSize AuthButtonIconSize(24, 24);

SFAWidget::SFAWidget(QWidget *parent)
    : AuthWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_chooseAuthButtonBox(nullptr)
    , m_biometricAuthState(nullptr)
    , m_retryButton(new DFloatingButton(this))
    , m_bioAuthStatePlaceHolder(new QSpacerItem(0, BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT))
    , m_bioBottomSpacingHolder(new QSpacerItem(0, calcCurrentHeight(BIO_AUTH_STATE_BOTTOM_SPACING)))
    , m_authTypeBottomSpacingHolder(new QSpacerItem(0, calcCurrentHeight(CHOOSE_AUTH_TYPE_BUTTON_BOTTOM_SPACING)))
    , m_currentAuthType(AT_All)
{
    setObjectName(QStringLiteral("SFAWidget"));
    setAccessibleName(QStringLiteral("SFAWidget"));

    setGeometry(0, 0, 280, 176);
    setMinimumSize(280, 176);
}

SFAWidget::~SFAWidget()
{

}

void SFAWidget::initUI()
{
    AuthWidget::initUI();
    /* 认证选择 */
    m_chooseAuthButtonBox = new ButtonBox(this);
    m_chooseAuthButtonBox->setFocusPolicy(Qt::NoFocus);
    m_chooseAuthButtonBox->setContentsMargins(0, 0, 0, 0);
    m_chooseAuthButtonBox->setFixedHeight(CHOOSE_AUTH_TYPE_PLACE_HOLDER_HEIGHT);

    /* 生物认证状态 */
    m_biometricAuthState = new DLabel(this);
    m_biometricAuthState->setFixedHeight(BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT);
    m_biometricAuthState->hide();

    /* 重试按钮 */
    m_retryButton->setIcon(QIcon(":/img/bottom_actions/reboot.svg"));
    m_retryButton->hide();

    m_mainLayout->setContentsMargins(10, 0, 10, 0);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_biometricAuthState, 0, Qt::AlignCenter);
    m_mainLayout->addItem(m_bioAuthStatePlaceHolder);
    m_mainLayout->addItem(m_bioBottomSpacingHolder);
    m_mainLayout->addWidget(m_chooseAuthButtonBox, 0, Qt::AlignCenter);
    m_mainLayout->addItem(m_authTypeBottomSpacingHolder);
    SpacerItemBinder::addWidget(m_userAvatar, m_mainLayout, Qt::AlignVCenter, 5);
    SpacerItemBinder::addWidget(m_accountEdit, m_mainLayout, Qt::AlignVCenter, 20);
    SpacerItemBinder::addWidget(m_userNameWidget, m_mainLayout, Qt::AlignVCenter, 20);
    m_mainLayout->addSpacing(20);
    m_mainLayout->addWidget(m_expiredStateLabel);
    m_mainLayout->addSpacing(10);
    SpacerItemBinder::addWidget(m_lockButton, m_mainLayout, Qt::AlignCenter);
    SpacerItemBinder::addWidget(m_retryButton, m_mainLayout, Qt::AlignCenter);

    m_mainLayout->invalidate();
}

void SFAWidget::initConnections()
{
    AuthWidget::initConnections();
    connect(m_model, &SessionBaseModel::authTypeChanged, this, &SFAWidget::setAuthType);
    connect(m_model, &SessionBaseModel::authStateChanged, this, &SFAWidget::setAuthState);
    connect(m_model, &SessionBaseModel::lightdmPamStartedChanged, this, &SFAWidget::onLightdmPamStartChanged);
    connect(m_model, &SessionBaseModel::accountError, this, &SFAWidget::onAccountError);
    connect(m_accountEdit, &DLineEditEx::textChanged, this, [this](const QString &value) {
        m_lockButton->setEnabled(!value.isEmpty());
    });
#if DTK_VERSION < DTK_VERSION_CHECK(6, 0, 0, 0)
    connect(SpacerItemBinder::instance(), &SpacerItemBinder::requestInvalidateLayout, m_mainLayout, &QVBoxLayout::invalidate);
#else
    connect(&SpacerItemBinder::ref(), &SpacerItemBinder::requestInvalidateLayout, m_mainLayout, &QVBoxLayout::invalidate);
#endif
    connect(PluginManager::instance(), &PluginManager::pluginAboutToBeRemoved, this, [this] (const QString &key) {
        qCInfo(DDE_SHELL) << key << " about to be removed, destruct custom auth now";
        auto it = m_customAuths.find(key);
        if (it == m_customAuths.end()) {
            qCWarning(DDE_SHELL) << "Can not find plugin:" << key;
            return;
        }
        const auto &auth = it.value();
        if (!auth) {
            qCWarning(DDE_SHELL) << "Custom auth object is nullptr";
            return;
        }
        const bool isCurrentAuthCustom = currentAuthCustom() == auth;
        m_chooseAuthButtonBox->removeButton(m_authButtons.value(auth->customAuthType()));
        removeAuthButton(auth);
        m_customAuths.remove(auth->getLoginPlugin()->key());
        auth->detachPlugin();
        auth->deleteLater();
        if (isCurrentAuthCustom) {
            qCInfo(DDE_SHELL) << key << " is current custom auth, change to first auth";
            if (m_authButtons.size() >= 1) {
                emit m_chooseAuthButtonBox->button(m_authButtons.firstKey())->toggled(true);
            } else {
                qCritical(DDE_SHELL) << "No other auth left, can not change to first auth";
            }
        }
    });
}

void SFAWidget::setModel(const SessionBaseModel *model)
{
    AuthWidget::setModel(model);

    initUI();
    initConnections();

    // 在登陆设置验证类型的时候需要判断当前是否是"..."账户，需要先设置当前用户，在设置验证类型，两者的顺序切勿颠倒。
    setUser(model->currentUser());
    setAuthType(model->getAuthProperty().AuthType);
}

/**
 * @brief 设置认证类型
 * @param type  认证类型
 */
void SFAWidget::setAuthType(const AuthFlags type)
{
    qCInfo(DDE_SHELL) << "Auth type:" << type;
    AuthFlags authType = type;
    // 如果密码已过期且当前用户不允许修改密码，则将认证类型改为密码认证
    if (!m_model->currentUser()->allowToChangePassword() && m_model->appType() == Login) {
        qCInfo(DDE_SHELL) << "Password is expired, current user is not allowed to change the password, set authentication type to `AT_Password`";
        authType = AT_Password;
    }

    // 隐藏生物认证结果状态图标
    m_biometricAuthState->hide();

    // 初始化认证因子
    initAuthFactors(type);

    // 初始化认证选择按钮组
    initChooseAuthButtonBox(type);

    // 选择默认认证的类型
    chooseAuthType(authType);

    // 如果是无密码认证，焦点默认在解锁按钮上面
    if (m_model->currentUser()->isNoPasswordLogin()) {
        m_lockButton->setEnabled(true);
        setFocusProxy(m_lockButton);
        setFocus();
    }

    // 界面创建完毕后调整页面高度
    updateSpaceItem();
}

/**
 * @brief 初始化自定义认证因子
 *
 * @param type 当前支持的认证因子
 * @return int 如果自定义认证因子可用，那么在入参的type基础上增加自定义认证因子
 */
void SFAWidget::initCustomFactors(const AuthFlags type)
{
    PluginManager::instance()->broadcastAuthFactors(type);
    auto plugins = filtrateAuthPlugins(PluginManager::instance()->getLoginPlugins());
    qCInfo(DDE_SHELL) << "Login plugin size:" << plugins.size();
    if (!m_model->terminalLocked() && !plugins.isEmpty() && (type & AT_Custom)) {
        for (const auto plugin : plugins) {
            if (plugin) {
                plugin->updateConfig();
                // 手势认证当前还未适配单因认证
                if (plugin->defaultAuthType() & AT_Pattern) {
                    continue;
                }

                initCustomAuth(plugin);
            }
        }

        // 析构不使用的插件
        for (const auto &auth : m_customAuths.values()) {
            if (!plugins.contains(auth->getLoginPlugin())) {
                qCInfo(DDE_SHELL) << "Remove custom auth:" << auth->getLoginPlugin()->key();
                removeAuthButton(auth);
                m_customAuths.remove(auth->getLoginPlugin()->key());
                auth->deleteLater();
            }
        }
        // 增加输入账户名的界面
        if (type == AT_None && m_user->type() == User::Default) {
            initAccount();
        }
    } else {
        qCInfo(DDE_SHELL) << "Remove all custom auths";
        for (const auto& auth : m_customAuths.values())
            removeAuthButton(auth);
        qDeleteAll(m_customAuths);
        m_customAuths.clear();

        if (m_customAuth) {
            delete m_customAuth;
            m_customAuth = nullptr;
        }

        // fix152437，避免初始化其他认证方式图像和nameLabel为隐藏
        m_userAvatar->setVisible(true);
        m_lockButton->setVisible(true);
        if (m_user->type() != User::Default)
            m_userNameWidget->setVisible(true);
    }

    if (type != AT_None || m_customAuths.isEmpty() || m_user->type() != User::Default) {
        auto btn = m_authButtons.value(AT_None);
        if (btn) {
            btn->deleteLater();
            btn = nullptr;
            m_authButtons.remove(AT_None);
        }
    }
}

/**
 * @brief 初始化认证因子
 *
 * @param type 从DA传入的可支持的认证因子
 * @return int 如果自定义认证因子可用，那么在入参的type基础上增加自定义认证因子
 */
void SFAWidget::initAuthFactors(const AuthFlags authFactors)
{
    initCustomFactors(authFactors);

    auto updateAuthType = [this, authFactors](AuthType type,
                                           AuthModule *authenticator,
                                           void (SFAWidget::*initAuthFunc)()) {
        if (authFactors.testFlag(type)) {
            // After passing face or iris verification, if there is a timeout before entering the desktop, re-verification is required
            (this->*initAuthFunc)();
        } else if (authenticator) {
            delete authenticator;
            authenticator = nullptr;
            m_authButtons.value(type)->deleteLater();
            m_authButtons.remove(type);
        }
    };

    updateAuthType(AT_Password, m_passwordAuth, &SFAWidget::initPasswdAuth);
    updateAuthType(AT_Face, m_faceAuth, &SFAWidget::initFaceAuth);
    updateAuthType(AT_Iris, m_irisAuth, &SFAWidget::initIrisAuth);
    updateAuthType(AT_Fingerprint, m_fingerprintAuth, &SFAWidget::initFingerprintAuth);
    updateAuthType(AT_Passkey, m_passkeyAuth, &SFAWidget::initPasskeyAuth);
    updateAuthType(AT_Ukey, m_ukeyAuth, &SFAWidget::initUKeyAuth);
    updateAuthType(AT_PAM, m_singleAuth, &SFAWidget::initSingleAuth);
}

/**
 * @brief 设置认证状态
 * @param type      认证类型
 * @param state    认证状态
 * @param message   认证消息
 */
void SFAWidget::setAuthState(const AuthType type, const AuthState state, const QString &message)
{
    qCInfo(DDE_SHELL) << "Set auth state, auth type:" << type
            << ", state:" << state
            << ", message:" << message;
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
    case AT_Face:
        if (m_faceAuth) {
            m_faceAuth->setAuthState(state, message);
        }
        break;
    case AT_Ukey:
        if (m_ukeyAuth) {
            m_ukeyAuth->setAuthState(state, message);
        }
        break;
    case AT_Iris:
        if (m_irisAuth) {
            m_irisAuth->setAuthState(state, message);
        }
        break;
    case AT_Passkey:
        if (m_passkeyAuth) {
            m_passkeyAuth->setAuthState(state, message);
        }
        break;
    case AT_PAM:
        if (m_singleAuth) {
            m_singleAuth->setAuthState(state, message);
        } else if (m_passwordAuth) {
            m_passwordAuth->setAuthState(state, message);
        }

        // 这里是为了让自定义登陆知道lightdm已经开启验证了
        qCInfo(DDE_SHELL) << "Current auth type is: " << m_currentAuthType;
        if (currentAuthCustom() && state == AS_Prompt) {
            // 有可能DA发送了验证开始，但是lightdm的验证还未开始，此时发送token的话lightdm无法验证通过。
            // lightdm的pam发送prompt后则认为lightdm验证已经开始
            qCInfo(DDE_SHELL) << "LightDM is in authentication right now";
            currentAuthCustom()->lightdmAuthStarted();
        }

        if (m_passwordAuth && m_passwordAuth->isPasswdAuthWidgetReplaced()) {
            qCDebug(DDE_SHELL) << Q_FUNC_INFO << type << state << message << "startAuth";
            m_passwordAuth->startPluginAuth();
        }
        break;
    case AT_Custom:
        if (currentAuthCustom()) {
            currentAuthCustom()->setAuthState(state, message);
        }
        break;
    case AT_All:
        checkAuthResult(AT_All, state);
        break;
    default:
        break;
    }

    // 同步验证状态给插件
    if (currentAuthCustom()) {
        currentAuthCustom()->notifyAuthState(type, state);
    }
}

/**
 * @brief 初始化单因认证
 * 用于兼容开源 PAM
 */
void SFAWidget::initSingleAuth()
{
    if (m_singleAuth) {
        m_singleAuth->reset();
        return;
    }
    m_singleAuth = new AuthSingle(this);
    m_singleAuth->setCurrentUid(m_model->currentUser()->uid());
    replaceWidget(m_singleAuth);
    m_singleAuth->setPasswordLineEditEnabled(m_model->currentUser()->allowToChangePassword() || m_model->appType() != Login);

    connect(m_singleAuth, &AuthSingle::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_singleAuth, &AuthSingle::authFinished, this, [this](const int authState) {
        if (authState == AS_Success) {
            m_user->setLastAuthType(AT_PAM);
            m_lockButton->setEnabled(true);
            emit authFinished();
        }
    });
    connect(m_singleAuth, &AuthSingle::requestAuthenticate, this, [this] {
        if (m_singleAuth->lineEditText().isEmpty()) {
            return;
        }
        emit sendTokenToAuth(m_model->currentUser()->name(), AT_PAM, m_singleAuth->lineEditText());
    });
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged, m_singleAuth, &AuthSingle::setCapsLockVisible);
    connect(m_lockButton, &QPushButton::clicked, m_singleAuth, &AuthSingle::requestAuthenticate);

    connect(m_singleAuth, &AuthSingle::lineEditTextChanged, this, [this](const QString &value) {
        m_lockButton->setEnabled(!value.isEmpty());
    });

    m_singleAuth->setKeyboardButtonVisible(m_keyboardList.size() > 1 ? true : false);
    m_singleAuth->setKeyboardButtonInfo(m_keyboardType);
    m_singleAuth->setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());
    m_singleAuth->setPasswordHint(m_model->currentUser()->passwordHint());

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Password_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_PAM, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_singleAuth);
            m_biometricAuthState->hide();
            setBioAuthStateVisible(nullptr, false);
            emit requestStartAuthentication(m_user->name(), AT_PAM);
        } else {
            m_singleAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AT_PAM);
        }
    });
}

/**
 * @brief 初始化密码认证
 */
void SFAWidget::initPasswdAuth()
{
    if (m_passwordAuth) {
        m_passwordAuth->reset();
        return;
    }
    m_passwordAuth = new AuthPassword(this);
    m_passwordAuth->setCurrentUid(m_model->currentUser()->uid());
    m_passwordAuth->hide();
    m_passwordAuth->setPasswordLineEditEnabled(m_model->currentUser()->allowToChangePassword() || m_model->appType() != Login);

    connect(m_passwordAuth, &AuthPassword::requestPluginConfigChanged, this, [ this ](const LoginPlugin::PluginConfig pluginConfig) {
        m_userAvatar->setVisible(pluginConfig.showAvatar);
        m_userNameWidget->setVisible(pluginConfig.showUserName);
        m_lockButton->setVisible(pluginConfig.showLockButton);
        replaceWidget(m_passwordAuth);
    });

    connect(m_passwordAuth, &AuthPassword::requestHidePlugin, this, [ this ] {
        m_userAvatar->setVisible(true);
        m_userNameWidget->setVisible(true);
        m_lockButton->setVisible(true);
        replaceWidget(m_passwordAuth);
        m_passwordAuth->updateResetPasswordUI();
    });

    connect(m_passwordAuth, &AuthPassword::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_passwordAuth, &AuthPassword::authFinished, this, [this] (const AuthState authState) {
        checkAuthResult(AT_Password, authState);
    });
    connect(m_passwordAuth, &AuthPassword::requestPluginAuthToken, this, [this](const QString & account, const QString & token) {
        m_passwordAuth->setAuthStateStyle(LOGIN_SPINNER);
        m_passwordAuth->setAnimationState(true);
        m_passwordAuth->setLineEditEnabled(false);
        m_lockButton->setEnabled(false);
        qCInfo(DDE_SHELL) << "Request check account: " << account << token;
        if (m_user && m_user->name() == account) {
            emit sendTokenToAuth(account, AT_Password, token);
            return;
        }

        Q_EMIT requestCheckAccount(account);
    });
    connect(m_passwordAuth, &AuthPassword::requestAuthenticate, this, [this] {
        QString text = m_passwordAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        if (m_passwordAuth->assistLoginWidget()) {
            if (m_user) {
                Q_EMIT SignalBridge::ref().requestSendExtraInfo(m_user->name(), AT_Password, m_passwordAuth->assistLoginWidget()->extraInfo());
            } else {
                qCWarning(DDE_SHELL) << "Current user is null, can not send extra info";
            }
        }
        m_passwordAuth->setAuthStateStyle(LOGIN_SPINNER);
        m_passwordAuth->setAnimationState(true);
        m_passwordAuth->setLineEditEnabled(false);
        m_lockButton->setEnabled(false);
        // OPTIMIZING: DA没有发AS_Verify，这里自己发一个，后续DA优化
        m_passwordAuth->setAuthState(AuthCommon::AS_Verify, QStringLiteral("Verifying..."));

        if (m_model->currentUser()->isLdapUser()
            && LPUtil::updateLoginType() == LPUtil::Type::CLT_MFA)
        {
            QJsonObject obj;
            obj["password"] = text;
            obj["mfa"] = true;
            text = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        }

        emit sendTokenToAuth(m_user->name(), AT_Password, text);
    });

    connect(m_passwordAuth, &AuthPassword::notifyLockedStateChanged, this, [this](bool isLocked) {
        m_chooseAuthButtonBox->setEnabled(!isLocked);
        if (isLocked) {
            onRequestChangeAuth(AT_Password);
        }
    });

    connect(m_passwordAuth, &AuthPassword::requestUpdateBlurEffectGeometry, this, &SFAWidget::updateBlurEffectGeometry);
    connect(m_lockButton, &QPushButton::clicked, m_passwordAuth, &AuthPassword::requestAuthenticate);
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged, m_passwordAuth, &AuthPassword::setCapsLockVisible);
    connect(m_passwordAuth, &AuthPassword::lineEditTextChanged, this, [this](const QString &value) {
        m_lockButton->setEnabled(!value.isEmpty());
    });
    m_passwordAuth->setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());
    m_passwordAuth->setPasswordHint(m_user->passwordHint());
    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Password_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Password, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_passwordAuth);
            m_biometricAuthState->hide();
            setBioAuthStateVisible(nullptr, false);
            emit requestStartAuthentication(m_user->name(), AT_Password);
            m_passwordAuth->updateResetPasswordUI();
        } else {
            m_passwordAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AT_Password);
            m_passwordAuth->closeResetPasswordMessage();
        }
    });

    if (m_passwordAuth->isPasswdAuthWidgetReplaced()) {
        if (m_model->appType() == AppType::Lock) {
            m_passwordAuth->hidePlugin();
            return;
        }
        m_passwordAuth->updatePluginConfig();
    }
}

/**
 * @brief 初始化指纹认证
 */
void SFAWidget::initFingerprintAuth()
{
    if (m_fingerprintAuth) {
        m_fingerprintAuth->reset();
        return;
    }
    m_fingerprintAuth = new AuthFingerprint(this);
    m_fingerprintAuth->hide();
    m_fingerprintAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);
    m_fingerprintAuth->setAuthStateLabel(m_biometricAuthState);

    connect(m_fingerprintAuth, &AuthFingerprint::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_fingerprintAuth, &AuthFingerprint::authFinished, this, [this](const AuthState authState) {
        checkAuthResult(AT_Fingerprint, authState);
    });

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Fingerprint_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Fingerprint, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_fingerprintAuth);
            setBioAuthStateVisible(m_fingerprintAuth, true);
            emit requestStartAuthentication(m_user->name(), AT_Fingerprint);
        } else {
            m_fingerprintAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AT_Fingerprint);
        }
    });
}

/**
 * @brief 初始化 Passkey 认证
 */
void SFAWidget::initPasskeyAuth()
{
    if (m_passkeyAuth) {
        m_passkeyAuth->reset();
        return;
    }

    m_passkeyAuth = new AuthPasskey(this);
    m_passkeyAuth->hide();
    m_passkeyAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);
    m_passkeyAuth->setAuthStateLabel(m_biometricAuthState);

    connect(m_passkeyAuth, &AuthPasskey::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_passkeyAuth, &AuthPasskey::authFinished, this, [this](const AuthState authState) {
        checkAuthResult(AT_Passkey, authState);
    });
    connect(m_passkeyAuth, &AuthPasskey::retryButtonVisibleChanged, this, &SFAWidget::onRetryButtonVisibleChanged);
    connect(m_retryButton, &DFloatingButton::clicked, this, [this] {
        if (m_currentAuthType != AT_Passkey) {
            return;
        }
        emit requestEndAuthentication(m_model->currentUser()->name(), AT_Passkey);
        onRetryButtonVisibleChanged(false);
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Passkey);
    });

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Face_Passkey), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Passkey, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_passkeyAuth);
            setBioAuthStateVisible(m_passkeyAuth, true);
            emit requestStartAuthentication(m_user->name(), AT_Passkey);
        } else {
            m_passkeyAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AT_Passkey);
        }
    });
}

/**
 * @brief 初始化 UKey 认证
 */
void SFAWidget::initUKeyAuth()
{
    if (m_ukeyAuth) {
        m_ukeyAuth->reset();
        return;
    }
    m_ukeyAuth = new AuthUKey(this);
    m_ukeyAuth->hide();

    connect(m_ukeyAuth, &AuthUKey::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_ukeyAuth, &AuthUKey::authFinished, this, [this](const AuthState authState) {
        checkAuthResult(AT_Ukey, authState);
    });
    connect(m_ukeyAuth, &AuthUKey::requestAuthenticate, this, [this] {
        const QString &text = m_ukeyAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        m_ukeyAuth->setAuthStateStyle(LOGIN_SPINNER);
        m_ukeyAuth->setAnimationState(true);
        m_ukeyAuth->setLineEditEnabled(false);
        m_lockButton->setEnabled(false);
        emit sendTokenToAuth(m_model->currentUser()->name(), AT_Ukey, text);
    });
    connect(m_lockButton, &QPushButton::clicked, m_ukeyAuth, &AuthUKey::requestAuthenticate);
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged, m_ukeyAuth, &AuthUKey::setCapsLockVisible);
    connect(m_ukeyAuth, &AuthUKey::lineEditTextChanged, this, [this](const QString &value) {
        if (m_model->getAuthProperty().PINLen > 0 && value.size() >= m_model->getAuthProperty().PINLen) {
            emit m_ukeyAuth->requestAuthenticate();
        }
    });

    m_ukeyAuth->setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(UKey_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Ukey, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_ukeyAuth);
            m_biometricAuthState->hide();
            setBioAuthStateVisible(nullptr, false);
            emit requestStartAuthentication(m_user->name(), AT_Ukey);
        } else {
            m_ukeyAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AT_Ukey);
        }
    });
}

/**
 * @brief 初始化人脸认证
 */
void SFAWidget::initFaceAuth()
{
    if (m_faceAuth) {
        m_faceAuth->reset();
        return;
    }
    m_faceAuth = new AuthFace(this);
    m_faceAuth->setAuthStateLabel(m_biometricAuthState);
    m_faceAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);
    m_faceAuth->hide();

    connect(m_faceAuth, &AuthFace::retryButtonVisibleChanged, this, &SFAWidget::onRetryButtonVisibleChanged);
    connect(m_retryButton, &DFloatingButton::clicked, this, [this] {
        if (m_currentAuthType != AT_Face) {
            return;
        }
        onRetryButtonVisibleChanged(false);
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Face);
    });
    connect(m_faceAuth, &AuthFace::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_faceAuth, &AuthFace::authFinished, this, [this](const AuthState authState) {
        checkAuthResult(AT_Face, authState);
    });
    connect(m_lockButton, &QPushButton::clicked, this, [this] {
        if (m_faceAuth == nullptr) return;
        if (m_faceAuth->authState() == AS_Success) {
            m_faceAuth->setAuthState(AS_Ended, "Ended");

            if (m_model->currentUser()->isLdapUser()
                && LPUtil::loginType() == LPUtil::Type::CLT_MFA)
            {
                initCustomMFAAuth();
            } else {
                emit authFinished();
            }
        }
    });

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Face_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Face, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_faceAuth);
            setBioAuthStateVisible(m_faceAuth, true);
            if (m_faceAuth->authState() != AS_Success) {
                emit requestStartAuthentication(m_user->name(), AT_Face);
            } else {
                m_lockButton->setEnabled(true);
            }
        } else {
            m_faceAuth->hide();
            if (m_faceAuth->authState() != AS_Success) {
                m_lockButton->setEnabled(false);
                emit requestEndAuthentication(m_user->name(), AT_Face);
            }
        }
    });
}

/**
 * @brief 初始化虹膜认证
 */
void SFAWidget::initIrisAuth()
{
    if (m_irisAuth) {
        m_irisAuth->reset();
        return;
    }
    m_irisAuth = new AuthIris(this);
    m_irisAuth->hide();
    m_irisAuth->setAuthStateLabel(m_biometricAuthState);
    m_irisAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);

    connect(m_irisAuth, &AuthIris::retryButtonVisibleChanged, this, &SFAWidget::onRetryButtonVisibleChanged);
    connect(m_retryButton, &DFloatingButton::clicked, this, [this] {
        if (m_currentAuthType != AT_Iris) {
            return;
        }
        onRetryButtonVisibleChanged(false);
        emit requestStartAuthentication(m_model->currentUser()->name(), AT_Iris);
    });
    connect(m_irisAuth, &AuthIris::activeAuth, this, &SFAWidget::onActiveAuth);
    connect(m_irisAuth, &AuthIris::authFinished, this, [this](const AuthState authState) {
        checkAuthResult(AT_Iris, authState);
    });
    connect(m_lockButton, &QPushButton::clicked, this, [this] {
        if (m_irisAuth == nullptr) return;
        if (m_irisAuth->authState() == AS_Success) {
            m_irisAuth->setAuthState(AS_Ended, "Ended");

            if (m_model->currentUser()->isLdapUser()
                && LPUtil::loginType() == LPUtil::Type::CLT_MFA) {
                initCustomMFAAuth();
            } else {
                emit authFinished();
            }

        }
    });

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Iris_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_Iris, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_irisAuth);
            setBioAuthStateVisible(m_irisAuth, true);
            if (m_irisAuth->authState() != AS_Success) {
                emit requestStartAuthentication(m_user->name(), AT_Iris);
            } else {
                m_lockButton->setEnabled(true);
            }
        } else {
            m_irisAuth->hide();
            if (m_irisAuth->authState() != AS_Success) {
                m_lockButton->setEnabled(false);
                emit requestEndAuthentication(m_user->name(), AT_Iris);
            }
        }
    });
}

void SFAWidget::initCustomMFAAuth()
{
    qCDebug(DDE_SHELL) << "Init custom auth";

    m_chooseAuthButtonBox->hide();
    if (m_passwordAuth) {
        m_passwordAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Password);
    }
    if (m_fingerprintAuth) {
        m_fingerprintAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Fingerprint);
    }
    if (m_faceAuth) {
        m_faceAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Face);
    }
    if (m_passkeyAuth) {
        m_passkeyAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Passkey);
    }
    if (m_irisAuth) {
        m_irisAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Iris);
    }
    if (m_ukeyAuth) {
        m_ukeyAuth->hide();
        emit requestEndAuthentication(m_user->name(), AT_Ukey);
    }
    if (!m_customAuths.isEmpty()) {
        for (const auto &auth : m_customAuths)
            auth->hide();
    }

    if (m_customAuth) {
        m_customAuth->reset();
        return;
    }

    m_customAuth = new AuthCustom(this);

    LoginPlugin *plugin = LPUtil::getLevel2LoginPlugin();
    m_customAuth->setModule(plugin);
    m_customAuth->setModel(m_model);
    m_customAuth->initUi();
    m_customAuth->hide();

    connect(m_customAuth, &AuthCustom::requestSendToken, this, [this] (const QString &token) {
        m_lockButton->setEnabled(false);
        qCInfo(DDE_SHELL) << "SFA custom send token name :" <<  m_user->name() << ", token:" << token;
        Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, token);
    });
    connect(m_customAuth, &AuthCustom::authFinished, this, [ this ] (const int authStatus) {
        if (authStatus == AS_Success) {
            m_lockButton->setEnabled(true);
        }
    });
    connect(m_customAuth, &AuthCustom::requestCheckAccount, this, [this] (const QString &account, bool switchUser) {
        qCInfo(DDE_SHELL) << "Request check account: " << account;
        if (m_user && m_user->name() == account) {
            LoginPlugin::AuthCallbackData data = m_customAuth->getCurrentAuthData();
            if (data.result == LoginPlugin::AuthResult::Success)
                Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, data.token);
            else
                qCWarning(DDE_SHELL) << Q_FUNC_INFO << "auth failed";
            return;
        }

        Q_EMIT requestCheckAccount(account, switchUser);
    });

    connect(m_customAuth, &AuthCustom::notifyAuthTypeChange, this, &SFAWidget::onRequestChangeAuth);

    m_biometricAuthState->hide();
    setBioAuthStateVisible(nullptr, false);
    m_accountEdit->hide();
    m_userAvatar->setVisible(m_customAuth->pluginConfig().showAvatar);
    m_userNameWidget->setVisible(m_customAuth->pluginConfig().showUserName);
    m_lockButton->setVisible(m_customAuth->pluginConfig().showLockButton);
    m_blurEffectWidget->setVisible(m_customAuth->pluginConfig().showBackGroundColor);
    replaceWidget(m_customAuth);
    Q_EMIT requestStartAuthentication(m_user->name(), AT_Custom);
}

/**
 * @brief 初始化自定义认证
 */
void SFAWidget::initCustomAuth(LoginPlugin* plugin)
{
    qCInfo(DDE_SHELL) << "Init custom auth";
    if (!plugin) {
        qCInfo(DDE_SHELL) << "Login plugin is nullptr";
        return;
    }
    auto it = m_customAuths.find(plugin->key());
    if (it != m_customAuths.end()) {
        auto customAuth = *it;
        if (customAuth) {
            customAuth->reset();
            qCInfo(DDE_SHELL) << "Auth plugin existed:" << customAuth->loginPluginKey();
            return;
        }
    }

    qCInfo(DDE_SHELL) << "Create new custom authentication with plugin:" << plugin->key();
    auto customAuth = new AuthCustom(this);
    customAuth->setModule(plugin);
    customAuth->setModel(m_model);
    customAuth->initUi();
    customAuth->hide();
    customAuth->setCustomAuthIndex(m_customAuths.count() + 1);
    m_blurEffectWidget->setVisible(customAuth->pluginConfig().showBackGroundColor);
    m_customAuths.insert(plugin->key(), customAuth);

    connect(customAuth, &AuthCustom::requestSendToken, this, [this, customAuth](const QString &token) {
        m_lockButton->setEnabled(false);
        qCInfo(DDE_SHELL) << customAuth->loginPluginKey() << " request send token, user name:" <<  m_user->name();
        Q_EMIT sendTokenToAuth(m_user->name(), customAuth->pluginConfig().assignAuthType, token);
    });
    connect(customAuth, &AuthCustom::authFinished, customAuth, [this, customAuth](const AuthState authState) {
        qCInfo(DDE_SHELL) << customAuth->loginPluginKey() <<  " auth finished, state:" << authState;
        checkAuthResult(AT_Custom, authState);
        if (m_user)
            m_user->setLastCustomAuth(customAuth->loginPluginKey());
    });
    connect(customAuth, &AuthCustom::requestCheckAccount, this, [this, customAuth](const QString &account, bool switchUser) {
        qCInfo(DDE_SHELL) << customAuth->loginPluginKey() << " request check account:" << account;
        if (m_user && m_user->name() == account) {
            LoginPlugin::AuthCallbackData data = customAuth->getCurrentAuthData();
            if (data.result == LoginPlugin::AuthResult::Success)
                Q_EMIT sendTokenToAuth(m_user->name(), customAuth->pluginConfig().assignAuthType, data.token);
            else
                qCWarning(DDE_SHELL) << "Custom auth failed";

            return;
        }

        Q_EMIT requestCheckAccount(account, switchUser);
    });

    connect(customAuth, &AuthCustom::notifyAuthTypeChange, this, &SFAWidget::onRequestChangeAuth);

    /* 认证选择按钮 */
    ButtonBoxButton *btn = new ButtonBoxButton(DStyle::SP_SelectElement, QString(), this);
    btn->setObjectName(plugin->key());
    const QString &iconStr = plugin->icon();
    const QIcon icon = QFile::exists(iconStr) ? QIcon(iconStr) : QIcon::fromTheme(iconStr);
    if (!icon.isNull())
        btn->setIcon(icon);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    const bool showBtn = plugin->pluginConfig().showSwitchButton;
    btn->setVisible(showBtn);
    btn->setVisibleAttr(showBtn);
    m_authButtons.insert(customAuth->customAuthType(), btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this, customAuth](const bool checked) {
        if (checked) {
            qCInfo(DDE_SHELL) << customAuth->loginPluginKey() << " is checked";
            m_biometricAuthState->hide();
            setBioAuthStateVisible(nullptr, false);
            m_accountEdit->hide();
            m_userAvatar->setVisible(customAuth->pluginConfig().showAvatar);
            m_userNameWidget->setVisible(customAuth->pluginConfig().showUserName);
            m_lockButton->setVisible(customAuth->pluginConfig().showLockButton);
            replaceWidget(customAuth);
            Q_EMIT requestStartAuthentication(m_user->name(), customAuth->pluginConfig().assignAuthType);
        } else {
            qCInfo(DDE_SHELL) << customAuth->loginPluginKey() << " is unchecked";
            customAuth->hide();
            m_userNameWidget->show();
            m_userAvatar->show();
            m_lockButton->show();
            m_lockButton->setEnabled(false);
            if (m_user->type() == User::Default) {
                m_userNameWidget->hide();
                m_accountEdit->show();
                m_accountEdit->setFocus();
            }
            Q_EMIT requestEndAuthentication(m_user->name(), customAuth->pluginConfig().assignAuthType);
        }
    });
}

/**
 * @brief SFAWidget::checkAuthResult
 *
 * @param type  认证类型
 * @param state 认证状态
 */
void SFAWidget::checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state)
{
    // 等所有类型验证通过的时候在发送验证完成信息，否则DA的验证结果可能还没有刷新，导致lightdm调用pam验证失败
    // 人脸和虹膜是手动点击解锁按钮后发送，无需处理
    if (type == AT_All && state == AS_Success) {
        if( m_model->appType() != AppType::Login || m_model->isLightdmPamStarted()){
            sendAuthFinished();
        }

        if (currentAuthCustom()) {
            currentAuthCustom()->resetAuth();
        }
    } else if (type != AT_All && state == AS_Success) {
        if (type == AuthCommon::AT_Custom ) {
            const auto customAuth = currentAuthCustom();
            if (customAuth && customAuth->pluginConfig().saveLastAuthType)
                m_user->setLastAuthType(type);
        } else {
            m_user->setLastAuthType(type);
        }
        m_lockButton->setEnabled(true);
        m_lockButton->setFocus();

        if (type == AT_Face || type == AT_Iris) {
            // 禁止切换其他认证方式
            m_chooseAuthButtonBox->setEnabled(false);
        }
    }
}

void SFAWidget::replaceWidget(AuthModule *authModule)
{
    m_currentAuthType = authModule->authType();
    if (authModule->authType() == AuthCommon::AT_Custom) {
        const auto customAuth = qobject_cast<AuthCustom*>(authModule);
        if (customAuth)
            m_currentAuthType = customAuth->customAuthType();
    }
    qCInfo(DDE_SHELL) << "Replace widget, current auth type:" << m_currentAuthType;
    m_mainLayout->insertWidget(layout()->indexOf(m_userNameWidget) + 2, authModule);
    authModule->show();
    setFocus();
    if (authModule->isLocked())
        authModule->clearFocus();
    else
        authModule->setFocus();
    onRetryButtonVisibleChanged(false);
    updateBlurEffectGeometry();
    if (m_currentAuthType == AT_PAM) {
        if (m_singleAuth && m_singleAuth->lineEditText() == "") {
            m_lockButton->setEnabled(false);
        }
    } else if (m_currentAuthType == AT_Ukey) {
        if (m_ukeyAuth && m_ukeyAuth->lineEditText() == "") {
            m_lockButton->setEnabled(false);
        }
    } else if (m_currentAuthType == AT_Password) {
        if (m_passwordAuth && m_passwordAuth->lineEditText() == "") {
            m_lockButton->setEnabled(false);
        }
    } else {
        m_lockButton->setEnabled(false);
    }

    setFocusProxy(authModule);
    // 加入事件循环中处理，否则其它控件的geometry还未设置完成，导致UI显示异常
    QTimer::singleShot(0, this, &SFAWidget::updateBlurEffectGeometry);
}

void SFAWidget::onRetryButtonVisibleChanged(bool visible)
{
    m_lockButton->setVisible(!visible && !(currentAuthCustom() && !currentAuthCustom()->pluginConfig().showLockButton));
    m_retryButton->setVisible(visible);
    visible ? m_retryButton->setFocus() : updateFocus();
}

void SFAWidget::setBioAuthStateVisible(AuthModule *authModule, bool visible)
{
    bool hasBioAuth = (m_faceAuth || m_fingerprintAuth || m_irisAuth || m_passkeyAuth);
    m_bioAuthStatePlaceHolder->changeSize(0, (visible || !hasBioAuth) ? 0 : BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT);
    if (authModule)
        authModule->setAuthStatueVisible(visible);

    m_mainLayout->invalidate();
}

/**
 * @brief 计算需要增加的顶部间隔，以实现用户头像到屏幕顶端的距离为屏幕高度的35%
 */
int SFAWidget::getTopSpacing() const
{
    const qreal pixelRatio = qApp->devicePixelRatio();
    const int topLevelWidgetHeight = topLevelWidget()->geometry().height() * pixelRatio;
    const int calcTopHeight = static_cast<int>(topLevelWidgetHeight * AUTH_WIDGET_TOP_SPACING_PERCENT);
    const bool showAuthButton = showAuthButtonBox();
    const bool hasBioAuth = (m_faceAuth || m_fingerprintAuth || m_irisAuth || m_passkeyAuth);
    // 在低分辨率（高度<916）的时候，如果用户头像到屏幕顶端的距离为整个高度的35%，那么验证窗口整体是偏下的。
    // 计算居中时的顶部高度，用来保证验证窗口最起码是居中（在分辨率非常低的时候也无法保证，根据测试768是没问题的）。
    int centerTop = static_cast<int>((topLevelWidgetHeight - MIN_AUTH_WIDGET_HEIGHT) / 2);
    if (!m_customAuths.isEmpty()) {
        // 一般自定义的类型控件尺寸都会比较大，尽量保证能够居中显示。
        // 根据头像、用户名、锁屏以及插件自身的高度来计算居中时顶部间隔
        // 这是一个相对比较严格的高度，如果插件还是无法显示完整，或者界面偏下的话，只能插件调整content界面的大小了
        bool showAvatar = false;
        bool showUserName = false;
        bool showLockButton = false;
        int maxContentHeight = 0;
        for (const auto &auth : m_customAuths) {
            const auto &pluginConfig = auth->pluginConfig();
            showAvatar = showAvatar || pluginConfig.showAvatar;
            showUserName = showUserName || pluginConfig.showUserName;
            showLockButton = showLockButton || pluginConfig.showLockButton;
            maxContentHeight = qMax(maxContentHeight, static_cast<int>(auth->contentSizeHint().height() * pixelRatio));
        }

        int height = showAvatar ? m_userAvatar->height() + 10 : 0;
        height += showUserName ? m_userNameWidget->height() + 10 : 0;
        height += showLockButton ? m_lockButton->height() + 10 : 0;
        centerTop = static_cast<int>((topLevelWidgetHeight - height * pixelRatio - maxContentHeight) / 2);
// Just for debug
#if 0
    qCInfo(DDE_SHELL) << "Height:" << height << ", show avatar:" << showAvatar << ", show user name:" << showUserName << ", show lock button:" << showLockButton << ", show auth button:" << showAuthButton;
#endif
    }
    const int topHeight = qMin(calcTopHeight, centerTop);
    // 需要额外增加的顶部间隔高度 = 屏幕高度*0.35 - 布局间隔 - 生物认证按钮底部间隔
    // - 生物认证切换按钮底部间隔 - 生物认证图标高度(如果有生物认证因子) - 切换验证类型按钮高度（如果认证因子数量大于1)
    int deltaY = topHeight - calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN) * pixelRatio
            - (hasBioAuth ? BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT * pixelRatio : 0)
            - (hasBioAuth ? calcCurrentHeight(BIO_AUTH_STATE_BOTTOM_SPACING) * pixelRatio : 0)
            - (showAuthButton ? calcCurrentHeight(CHOOSE_AUTH_TYPE_BUTTON_BOTTOM_SPACING) * pixelRatio : 0)
            - (showAuthButton ? CHOOSE_AUTH_TYPE_PLACE_HOLDER_HEIGHT * pixelRatio : 0);

// Just for debug
#if 0
    qCInfo(DDE_SHELL) << "Top height:" << topHeight << ", center top:" << centerTop << ", deltaY:" << deltaY << ", pixelRatio:" << pixelRatio << ", show auth button:" << showAuthButton << ", hasBioAuth: " << hasBioAuth;
#endif

    deltaY = deltaY / pixelRatio;
    return qMax(15, deltaY);
}

void SFAWidget::resizeEvent(QResizeEvent *event)
{
    updateSpaceItem();
    QTimer::singleShot(0, this, &SFAWidget::updateBlurEffectGeometry);

    AuthWidget::resizeEvent(event);
}

/**
 * @brief 根据是否有多个因子，因子中是否含有生物认证来动态调整spaceItem的高度
 */
void SFAWidget::updateSpaceItem()
{
    m_authTypeBottomSpacingHolder->changeSize(0, showAuthButtonBox() ? calcCurrentHeight(CHOOSE_AUTH_TYPE_BUTTON_BOTTOM_SPACING) : 0);

    if (m_faceAuth || m_fingerprintAuth || m_irisAuth || m_passkeyAuth) {
        m_bioBottomSpacingHolder->changeSize(0, calcCurrentHeight(BIO_AUTH_STATE_BOTTOM_SPACING));
        m_bioAuthStatePlaceHolder->changeSize(0, m_bioAuthStatePlaceHolder->sizeHint().height() == 0 ? 0 : BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT);
    } else {
        m_bioBottomSpacingHolder->changeSize(0, 0);
        m_bioAuthStatePlaceHolder->changeSize(0, 0);
    }
    // QSpacerItem 调用了changeSize后需要其所属的layout调用一下invalidate，详见是qt的说明:
    // Note that if changeSize() is called after the spacer item has been added to a layout,
    // it is necessary to invalidate the layout in order for the spacer item's new size to take effect.
    m_mainLayout->invalidate();
    // 不是锁屏界面不重绘，会导致界面下移
    if (isVisible())
        emit updateParentLayout();
}

void SFAWidget::updateFocus()
{
    for (int i = 0; i < m_mainLayout->layout()->count(); ++i) {
        AuthModule *authModule = qobject_cast<AuthModule *>(m_mainLayout->layout()->itemAt(i)->widget());
        if (authModule && authModule->authType() == m_chooseAuthButtonBox->checkedId()) {
            if (currentAuthCustom() && !currentAuthCustom()->pluginConfig().showSwitchButton) {
                return;
            }
            setFocus();

            if (m_authState == AuthCommon::AS_Success && m_lockButton->isEnabled())
                m_lockButton->setFocus();
            else if (authModule->isLocked())
                authModule->clearFocus();
            else
                authModule->setFocus();

            break;
        }
    }
}

void SFAWidget::showEvent(QShowEvent *event)
{
    AuthWidget::showEvent(event);

    updateSpaceItem();
    QTimer::singleShot(0, this, &SFAWidget::updateBlurEffectGeometry);
    QTimer::singleShot(0, this, &SFAWidget::updateFocus);
}

void SFAWidget::updateBlurEffectGeometry()
{
    const auto currenAuthCustom = currentAuthCustom();
    if (currenAuthCustom) {
        QRect rect = this->rect();
        // top
        if (!currenAuthCustom->pluginConfig().showAvatar) {
            if (currenAuthCustom->pluginConfig().showUserName) {
                rect.setTop(m_userNameWidget->geometry().top() - 10);
            } else {
                rect.setTop(currenAuthCustom->geometry().top() - 10);
            }
        } else {
            rect.setTop(m_userAvatar->geometry().top() + m_userAvatar->height() / 2);
        }
        // bottom
        if (m_user->expiredState() == User::ExpiredNormal) {
            if (!currenAuthCustom->pluginConfig().showLockButton) {
                rect.setBottom(currenAuthCustom->geometry().bottom() + 10);
            } else {
                rect.setBottom(m_lockButton->geometry().top() - 20);
            }
        } else {
            rect.setBottom(m_expiredStateLabel->geometry().top() - 10);
        }
        m_blurEffectWidget->setGeometry(rect);
        m_blurEffectWidget->update();
    } else if (m_passwordAuth && m_passwordAuth->isPasswdAuthWidgetReplaced()) {
        QRect rect = this->rect();
        rect.setTop(m_userAvatar->geometry().top());
        rect.setBottom(m_lockButton->geometry().top() - 10);
        m_blurEffectWidget->setGeometry(rect);
        m_blurEffectWidget->update();
    } else {
        AuthWidget::updateBlurEffectGeometry();
    }
}

void SFAWidget::initAccount()
{
    qCDebug(DDE_SHELL) << "Init switch button of account";
    /* 认证选择按钮 */
    if (m_authButtons.contains(AT_None)) {
        return;
    }
    ButtonBoxButton *btn = new ButtonBoxButton(QIcon(Password_Auth), QString(), this);
    btn->setIconSize(AuthButtonIconSize);
    btn->setFixedSize(AuthButtonSize);
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AT_None, btn);
    connect(btn, &ButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            m_accountEdit->show();
            m_lockButton->show();
            m_currentAuthType = AT_None;
            setBioAuthStateVisible(nullptr, false);
        } else {
            m_accountEdit->hide();
        }
    });
}

void SFAWidget::onRequestChangeAuth(AuthType authType)
{
    qCInfo(DDE_SHELL) << "Request change auth, auth type: " << authType
            << ", choose auth button box is enabled: " << m_chooseAuthButtonBox->isEnabled()
            << ", current authentication type: " << m_currentAuthType;

    int authTypeTmp = authType;
    const auto customAuth = qobject_cast<AuthCustom*>(sender());
    if (customAuth && customAuth->customAuthType() != m_currentAuthType) {
        // 输出一下日志表明是哪个插件请求切换的，目前遇到的场景：锁屏时开启了密码认证，云桌面需要切换单点登录插件认证，自动解锁。
        qCInfo(DDE_SHELL) << "Custom auth mismatch with current auth type, custom auth type: " << customAuth->customAuthType();
    }

    if (authTypeTmp == m_currentAuthType) {
        qCInfo(DDE_SHELL) << "Current auth type is same with request auth type";
        return;
    }

    if (authTypeTmp != AuthCommon::AT_Password && !m_chooseAuthButtonBox->isEnabled()) {
        qCInfo(DDE_SHELL) << "Authentication button box is disabled and authentication type is not password, automatically switch to password authentication";
        authTypeTmp = AT_Password;
    }

    // 如果请求切换到AT_All 表明请求方无所谓切换到某种特定的类型，交由登录器来处理。
    // 登录器会切换到上次认证成功的类型。
    if (authTypeTmp == AuthCommon::AT_All) {
        qCInfo(DDE_SHELL) << "Request authentication type is AT_All, let the session-shell to handle it";
        if (m_user) {
            authTypeTmp = m_user->lastAuthType();
        } else {
            qCWarning(DDE_SHELL) << "No user, use first auth type";
            authTypeTmp = m_authButtons.firstKey();
        }
    }

    if (!m_authButtons.contains(authTypeTmp)) {
        qCWarning(DDE_SHELL) << "Authentication buttons do not contain the type";

        // 登录选项默认显示上一次认证方式，当不存在上一次认证，默认显示第一种认证方式
        QAbstractButton *button = nullptr;
        const auto lastAuthType = m_user->lastAuthType();
        qCInfo(DDE_SHELL) << "Last authtication type:" << lastAuthType;
        if (lastAuthType == AuthCommon::AT_Custom) {
            authTypeTmp = m_authButtons.firstKey();
            // 获取上次自定义认证的类型
            const auto &lastCustomAuth = m_user->lastCustomAuth();
            qCInfo(DDE_SHELL) << "Last custom auth:" << lastCustomAuth;
            if (!lastCustomAuth.isEmpty() && m_customAuths.contains(lastCustomAuth)) {
                const auto &it = m_customAuths.find(lastCustomAuth);
                if (it != m_customAuths.end() && it.value() != nullptr) {
                    // 排除重复使用了当前自定义认证类型的情况
                    if (!customAuth || customAuth->customAuthType() != it.value()->customAuthType())
                        authTypeTmp = it.value()->customAuthType();
                }
            }
            button = m_chooseAuthButtonBox->button(authTypeTmp);
        } else if (m_authButtons.contains(lastAuthType)) {
            button = m_chooseAuthButtonBox->button(lastAuthType);
        } else {
            button = m_chooseAuthButtonBox->button(m_authButtons.firstKey());
        }

        if (button) {
            button->setChecked(true);
        } else {
            qCWarning(DDE_SHELL) << "Authentication button is nullptr";
        }

        return ;
    }

    QAbstractButton *btn = m_chooseAuthButtonBox->button(authTypeTmp);
    if (!btn) {
        qCWarning(DDE_SHELL) << "The button of authentication is null";
        return;
    }

    btn->setChecked(true);
}

QList<LoginPlugin*> SFAWidget::filtrateAuthPlugins(const QList<LoginPlugin*> &plugins) const
{
    QList<LoginPlugin*> retList;
    if (plugins.isEmpty()) {
        qCInfo(DDE_SHELL) << "There is no login plugin";
        return retList;
    }

    // 无密码登录或者自动登录不使用自定义认证
    // 去掉自动登录的判定，详见bug176463
    if (m_model->currentUser()->isNoPasswordLogin()) {
        qCInfo(DDE_SHELL) << "No password login is enabled";
        return retList;
    }

    // 主账户登录，切换用户b(在greeter界面），回到主账户后删除b账户，这个时候greeter切换用户到主账户
    const bool isLoginedUserInGreeter = m_model->appType() == Login && m_user->isLogin();
    qCInfo(DDE_SHELL) << "Current user is logged in: " << isLoginedUserInGreeter;

    for (const auto plugin : plugins) {
        if (!plugin->isPluginEnabled())
            continue;
        if (isLoginedUserInGreeter && plugin->pluginConfig().notUsedByLoginedUserInGreeter)
            continue;
        retList.append(plugin);
    }

    // 判断认证插件是否支持"..."用户
    if (m_user && m_user->type() == User::Default) {
        auto tmpList = retList;
        retList.clear();
        for (const auto plugin : tmpList) {
            if (plugin->supportDefaultUser())
                retList.append(plugin);
        }
    }

    return retList;
}

AuthCustom *SFAWidget::currentAuthCustom() const
{
    if (m_customAuth)
        return m_customAuth;

    if (m_currentAuthType >= AuthCommon::AT_Custom && !m_customAuths.isEmpty()) {
        for (const auto &auth : m_customAuths.values()) {
            if (auth && auth->customAuthType() == m_currentAuthType)
                return auth;
        }
    }

    return nullptr;
}

bool SFAWidget::useCustomAuth() const
{
    return filtrateAuthPlugins(PluginManager::instance()->getLoginPlugins()).isEmpty();
}

void SFAWidget::onActiveAuth(AuthType authType)
{
    if (m_currentAuthType != authType) {
        qCWarning(DDE_SHELL) << "Active authentication mismatch the current authentication type";
        return;
    }

    emit requestStartAuthentication(m_model->currentUser()->name(), authType);
}

void SFAWidget::onAccountError()
{
    QList<QPointer<AuthCustom>> authCustomWidgets = m_customAuths.values();
    for (const QPointer<AuthCustom> &authCustonWidget : authCustomWidgets) {
        LoginPlugin *loginPlugin = authCustonWidget->getLoginPlugin();
        loginPlugin->accountError();
    }
}

void SFAWidget::initChooseAuthButtonBox(const AuthFlags authFlags)
{
    m_chooseAuthButtonBox->setEnabled(true);

    m_chooseAuthButtonBox->setVisible(m_authButtons.count() > 1);
    if (!m_authButtons.isEmpty()) {
        m_chooseAuthButtonBox->setButtonList(m_authButtons.values(), true);
        auto iter = m_authButtons.constBegin();
        while (iter != m_authButtons.constEnd()) {
            m_chooseAuthButtonBox->setId(iter.value(), iter.key());
            ++iter;
        }
    }

    m_chooseAuthButtonBox->setVisible(showAuthButtonBox());
}

void SFAWidget::chooseAuthType(const AuthFlags authFlags)
{
    if (m_authButtons.isEmpty())
        return;

    do
    {
        // 只有一种认证类型，直接使用
        if (m_authButtons.count() == 1) {
            qCInfo(DDE_SHELL) << "Only one authentication type, use first auth type";
            m_currentAuthType = m_authButtons.firstKey();
            break;
        }
        // 如果密码锁定，那么切换到密码认证且不允许切换认证类型
        if (m_passwordAuth && m_passwordAuth->isLocked()) {
            qCInfo(DDE_SHELL) << "Password is locked, use password authentication";
            m_chooseAuthButtonBox->setEnabled(false);
            m_user->setLastAuthType(AT_Password);
            m_currentAuthType = AT_Password;
            break;
        }
        // 自定义认证插件的认证顺序
        if (!m_customAuths.isEmpty()) {
            // order 的优先级最高
            int customAuth = -1;
            const auto& authOrder = DConfigHelper::instance()->getConfig(DDESESSIONCC::LOGIN_PLUGIN_AUTH_ORDER, {}).toStringList();
            qCInfo(DDE_SHELL) << "Login plugin auth order:" << authOrder;
            for (const auto &authType : authOrder) {
                if (m_customAuths.contains(authType)) {
                    customAuth = m_customAuths.value(authType)->customAuthType();
                    break;
                }
            }
            if (customAuth != -1) {
                m_currentAuthType = customAuth;
                break;
            }
            // 遍历自定义认证，找到级别最高的
            for (const auto &auth : m_customAuths) {
                if (auth->pluginConfig().defaultAuthLevel == LoginPlugin::DefaultAuthLevel::StrongDefault) {
                    customAuth = auth->customAuthType();
                    break;
                }
            }
            if (customAuth != -1) {
                m_currentAuthType = customAuth;
                break;
            }
        }
        // 使用上一次认证成功类型
        if (authFlags & m_user->lastAuthType()) {
            m_currentAuthType = m_user->lastAuthType();
            qCInfo(DDE_SHELL) << "Last auth type:" << m_currentAuthType;
            // 上一次认证成功的类型是自定义认证
            if (m_currentAuthType == AT_Custom) {
                if (!m_customAuths.isEmpty()) {
                    m_currentAuthType = (int)AT_Custom + 1;
                    // 获取上次自定义认证的类型
                    const auto lastCustomAuth = m_user->lastCustomAuth();
                    qCInfo(DDE_SHELL) << "Last custom auth:" << lastCustomAuth;
                    if (!lastCustomAuth.isEmpty() && m_customAuths.contains(lastCustomAuth)) {
                        auto it = m_customAuths.find(lastCustomAuth);
                        if (it != m_customAuths.end() && it.value() != nullptr) {
                            m_currentAuthType = it.value()->customAuthType();
                        }
                    }
                } else {
                    qCInfo(DDE_SHELL) << "No custom auth, use first auth type";
                    m_currentAuthType = m_authButtons.firstKey();
                }
            }
        }
        // 防呆处理，排除异常情况
        if (!(authFlags & m_currentAuthType) || m_currentAuthType == AT_All || !m_authButtons.contains(m_currentAuthType))
            m_currentAuthType = m_authButtons.firstKey();
    } while(0);

    qCInfo(DDE_SHELL) << "Change auth type:" << m_currentAuthType;
    if (m_chooseAuthButtonBox->checkedId() == m_currentAuthType) {
        emit m_chooseAuthButtonBox->button(m_currentAuthType)->toggled(true);
    } else {
        m_chooseAuthButtonBox->button(m_currentAuthType)->setChecked(true);
    }
}

bool SFAWidget::showAuthButtonBox() const
{
    int count = 0;
    for (const auto &btn : m_chooseAuthButtonBox->buttonBoxList())
        count += btn->testVisibleAttr() ? 1: 0;
    // 那么当认证因子大于1时才显示切换认证按钮组
    return count > 1;
}

void SFAWidget::onLightdmPamStartChanged()
{
    if (m_model->getAuthResult().authType == AT_All && m_model->getAuthResult().authState == AS_Success) {
        sendAuthFinished();
    }
}

void SFAWidget::sendAuthFinished()
{
    if ((m_passwordAuth && AS_Success == m_passwordAuth->authState())
        || (m_ukeyAuth && AS_Success == m_ukeyAuth->authState())
        || (m_fingerprintAuth && AS_Success == m_fingerprintAuth->authState())
        || (m_passkeyAuth && AS_Success == m_passkeyAuth->authState())
        || (currentAuthCustom() && currentAuthCustom()->authState() == AS_Success)) {
        if (m_faceAuth) m_faceAuth->setAuthState(AS_Ended, "Ended");
        if (m_irisAuth) m_irisAuth->setAuthState(AS_Ended, "Ended");
        // 认证拦截，接入域管二级以上因子
        if (m_model->currentUser()->isLdapUser() && LPUtil::loginType() == LPUtil::Type::CLT_MFA) {
            if (currentAuthCustom() && currentAuthCustom()->authState() == AS_Success) {
                emit authFinished();
            } else if (LPUtil::hasSecondLevel(m_model->currentUser()->name())){
                initCustomMFAAuth();
            } else {
                emit authFinished();
            }
        } else {
            emit authFinished();
        }
    }
}

void SFAWidget::removeAuthButton(AuthCustom *auth)
{
    if (!auth)
        return;

    auto btn = m_authButtons.value(auth->customAuthType());
    if (btn) {
        btn->deleteLater();
        m_authButtons.remove(auth->customAuthType());
    }
}
