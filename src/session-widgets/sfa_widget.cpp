/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sfa_widget.h"

#include "auth_face.h"
#include "auth_fingerprint.h"
#include "auth_iris.h"
#include "auth_password.h"
#include "auth_single.h"
#include "auth_ukey.h"
#include "dlineeditex.h"
#include "framedatabind.h"
#include "kblayoutwidget.h"
#include "keyboardmonitor.h"
#include "sessionbasemodel.h"
#include "useravatar.h"

#include <DFontSizeManager>
#include <DHiDPIHelper>

SFAWidget::SFAWidget(QWidget *parent)
    : AuthWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_currentAuth(new AuthModule(this))
    , m_lastAuth(nullptr)
    , m_retryButton(new DFloatingButton(this))
{
    setObjectName(QStringLiteral("SFAWidget"));
    setAccessibleName(QStringLiteral("SFAWidget"));

    setGeometry(0, 0, 280, 176);
    setMinimumSize(280, 176);
}

void SFAWidget::initUI()
{
    AuthWidget::initUI();
    /* 用户名输入框 */
    std::function<void(QVariant)> accountChanged = std::bind(&SFAWidget::syncAccount, this, std::placeholders::_1);
    m_registerFunctions["SFAAccount"] = m_frameDataBind->registerFunction("SFAAccount", accountChanged);
    m_frameDataBind->refreshData("SFAAccount");
    /* 认证选择 */
    m_chooesAuthButtonBox = new DButtonBox(this);
    m_chooesAuthButtonBox->setOrientation(Qt::Horizontal);
    m_chooesAuthButtonBox->setFocusPolicy(Qt::NoFocus);
    /* 生物认证状态 */
    m_biometricAuthStatus = new DLabel(this);
    m_biometricAuthStatus->hide();
    /* 重试按钮 */
    m_retryButton->setIcon(QIcon(":/img/bottom_actions/reboot.svg"));
    m_retryButton->hide();

    m_mainLayout->setContentsMargins(10, 0, 10, 0);
    m_mainLayout->setSpacing(10);
    m_mainLayout->addWidget(m_biometricAuthStatus, 0, Qt::AlignCenter);
    m_mainLayout->addSpacing(50);
    m_mainLayout->addWidget(m_chooesAuthButtonBox, 0, Qt::AlignCenter);
    m_mainLayout->addSpacing(50);
    m_mainLayout->addWidget(m_userAvatar);
    m_mainLayout->addWidget(m_nameLabel, 0, Qt::AlignVCenter);
    m_mainLayout->addWidget(m_accountEdit, 0, Qt::AlignVCenter);
    m_mainLayout->addWidget(m_currentAuth);
    m_mainLayout->addSpacing(10);
    m_mainLayout->addWidget(m_expiredStatusLabel);
    m_mainLayout->addItem(m_expiredSpacerItem);
    m_mainLayout->addWidget(m_lockButton, 0, Qt::AlignCenter);
    m_mainLayout->addWidget(m_retryButton, 0, Qt::AlignCenter);
}

void SFAWidget::initConnections()
{
    AuthWidget::initConnections();
    connect(m_model, &SessionBaseModel::authTypeChanged, this, &SFAWidget::setAuthType);
    connect(m_model, &SessionBaseModel::authStatusChanged, this, &SFAWidget::setAuthStatus);
    connect(m_accountEdit, &DLineEditEx::textChanged, this, [this](const QString &value) {
        m_frameDataBind->updateValue(QStringLiteral("SFAAccount"), value);
    });
}

void SFAWidget::setModel(const SessionBaseModel *model)
{
    AuthWidget::setModel(model);

    initUI();
    initConnections();

    setAuthType(model->getAuthProperty().AuthType);
    setUser(model->currentUser());
}

/**
 * @brief 设置认证类型
 * @param type  认证类型
 */
void SFAWidget::setAuthType(const int type)
{
    qDebug() << "SFAWidget::setAuthType:" << type;
    if (type & AuthTypePassword) {
        initPasswdAuth();
    } else if (m_passwordAuth) {
        m_passwordAuth->deleteLater();
        m_passwordAuth = nullptr;
        m_authButtons.value(AuthTypePassword)->deleteLater();
        m_authButtons.remove(AuthTypePassword);
        m_frameDataBind->clearValue("SFPasswordAuthStatus");
        m_frameDataBind->clearValue("SFPasswordAuthMsg");
    }
    if (type & AuthTypeFace) {
        initFaceAuth();
    } else if (m_faceAuth) {
        m_faceAuth->deleteLater();
        m_faceAuth = nullptr;
        m_authButtons.value(AuthTypeFace)->deleteLater();
        m_authButtons.remove(AuthTypeFace);
        m_frameDataBind->clearValue("SFFaceAuthStatus");
        m_frameDataBind->clearValue("SFFaceAuthMsg");
    }
    if (type & AuthTypeIris) {
        initIrisAuth();
    } else if (m_irisAuth) {
        m_irisAuth->deleteLater();
        m_irisAuth = nullptr;
        m_authButtons.value(AuthTypeIris)->deleteLater();
        m_authButtons.remove(AuthTypeIris);
        m_frameDataBind->clearValue("SFIrisAuthStatus");
        m_frameDataBind->clearValue("SFIrisAuthMsg");
    }
    if (type & AuthTypeFingerprint) {
        initFingerprintAuth();
    } else if (m_fingerprintAuth) {
        m_fingerprintAuth->deleteLater();
        m_fingerprintAuth = nullptr;
        m_authButtons.value(AuthTypeFingerprint)->deleteLater();
        m_authButtons.remove(AuthTypeFingerprint);
        m_frameDataBind->clearValue("SFFingerprintAuthStatus");
        m_frameDataBind->clearValue("SFFingerprintAuthMsg");
    }
    if (type & AuthTypeUkey) {
        initUKeyAuth();
    } else if (m_ukeyAuth) {
        m_ukeyAuth->deleteLater();
        m_ukeyAuth = nullptr;
        m_authButtons.value(AuthTypeUkey)->deleteLater();
        m_authButtons.remove(AuthTypeUkey);
        m_frameDataBind->clearValue("SFUKeyAuthStatus");
        m_frameDataBind->clearValue("SFUKeyAuthMsg");
    }

    int count = 0;
    int typeTmp = type;
    while (typeTmp) {
        typeTmp &= typeTmp - 1;
        count++;
    }

    if (count > 1) {
        m_chooesAuthButtonBox->show();
        m_chooesAuthButtonBox->setButtonList(m_authButtons.values(), true);
        QMap<int, DButtonBoxButton *>::const_iterator iter = m_authButtons.constBegin();
        while (iter != m_authButtons.constEnd()) {
            m_chooesAuthButtonBox->setId(iter.value(), iter.key());
            iter.value()->show();
            ++iter;
        }

        if (m_lastAuth) {
            emit requestStartAuthentication(m_user->name(), m_lastAuth->authType());
        } else {
            m_chooesAuthButtonBox->button(m_authButtons.firstKey())->setChecked(true);
            m_chooesAuthButtonBox->button(m_authButtons.firstKey())->toggled(true);
        }

        std::function<void(QVariant)> authTypeChanged = std::bind(&SFAWidget::syncAuthType, this, std::placeholders::_1);
        m_registerFunctions["SFAType"] = m_frameDataBind->registerFunction("SFAType", authTypeChanged);
        m_frameDataBind->refreshData("SFAType");
    } else {
        if (!m_authButtons.isEmpty() && m_authButtons.first()) {
            m_authButtons.first()->hide();
            m_authButtons.first()->toggled(true);
        }
        m_chooesAuthButtonBox->hide();
    }
    m_lockButton->setEnabled(false);
}

/**
 * @brief 设置认证状态
 * @param type      认证类型
 * @param status    认证状态
 * @param message   认证消息
 */
void SFAWidget::setAuthStatus(const int type, const int status, const QString &message)
{
    qDebug() << "SFAWidget::setAuthStatus:" << type << status << message;
    switch (type) {
    case AuthTypePassword:
        if (m_passwordAuth) {
            m_passwordAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFPasswordAuthStatus", status);
            m_frameDataBind->updateValue("SFPasswordAuthMsg", message);
        }
        break;
    case AuthTypeFingerprint:
        if (m_fingerprintAuth) {
            m_fingerprintAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFFingerprintAuthStatus", status);
            m_frameDataBind->updateValue("SFFingerprintAuthMsg", message);
        }
        break;
    case AuthTypeFace:
        if (m_faceAuth) {
            m_faceAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFFaceAuthStatus", status);
            m_frameDataBind->updateValue("SFFaceAuthMsg", message);
        }
        break;
    case AuthTypeUkey:
        if (m_ukeyAuth) {
            m_ukeyAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFUKeyAuthStatus", status);
            m_frameDataBind->updateValue("SFUKeyAuthMsg", message);
        }
        break;
    case AuthTypeIris:
        if (m_irisAuth) {
            m_irisAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFIrisAuthStatus", status);
            m_frameDataBind->updateValue("SFIrisAuthMsg", message);
        }
        break;
    case AuthTypeSingle:
        if (m_singleAuth) {
            m_singleAuth->setAuthStatus(status, message);
            m_frameDataBind->updateValue("SFSingleAuthStatus", status);
            m_frameDataBind->updateValue("SFSingleAuthMsg", message);
        }
        break;
    case AuthTypeAll:
        // 等所有类型验证通过的时候在发送验证完成信息，否则pam从DA获取验证结果的时候会返回错误
        if ((m_passwordAuth && StatusCodeSuccess == m_passwordAuth->authStatus())
                || (m_ukeyAuth && StatusCodeSuccess == m_ukeyAuth->authStatus())
                || (m_fingerprintAuth && StatusCodeSuccess == m_fingerprintAuth->authStatus()))
            emit authFinished();

        break;
    default:
        break;
    }
}

void SFAWidget::syncResetPasswordUI()
{
    if (m_singleAuth) {
        m_singleAuth->updateResetPasswordUI();
    }
    if (m_passwordAuth) {
        m_passwordAuth->updateResetPasswordUI();
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
    m_singleAuth->hide();

    connect(m_singleAuth, &AuthSingle::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeSingle);
    });
    connect(m_singleAuth, &AuthSingle::requestAuthenticate, this, [this] {
        if (m_singleAuth->lineEditText().isEmpty()) {
            return;
        }
        emit sendTokenToAuth(m_model->currentUser()->name(), AuthTypeSingle, m_singleAuth->lineEditText());
    });
    connect(m_singleAuth, &AuthSingle::requestShowKeyboardList, this, &SFAWidget::showKeyboardList);
    connect(m_keyboardTypeWidget, &KbLayoutWidget::setButtonClicked, m_singleAuth, &AuthSingle::setKeyboardButtonInfo);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_singleAuth, &AuthSingle::setCapsLockVisible);
    connect(m_lockButton, &QPushButton::clicked, m_singleAuth, &AuthSingle::requestAuthenticate);
    /* 输入框数据同步（可能是密码或PIN） */
    std::function<void(QVariant)> tokenChanged = std::bind(&SFAWidget::syncSingle, this, std::placeholders::_1);
    registerSyncFunctions("SFSingleAuth", tokenChanged);
    connect(m_singleAuth, &AuthSingle::lineEditTextChanged, this, [ this ](const QString &value) {
        m_frameDataBind->updateValue("SFSingleAuth", value);
        m_lockButton->setEnabled(!value.isEmpty());
    });

    /* 重置密码可见性数据同步 */
    std::function<void(QVariant)> resetPasswordVisibleChanged = std::bind(&SFAWidget::syncSingleResetPasswordVisibleChanged, this, std::placeholders::_1);
    registerSyncFunctions("ResetPasswordVisible", resetPasswordVisibleChanged);
    connect(m_singleAuth, &AuthSingle::resetPasswordMessageVisibleChanged, this, [ = ](const bool value) {
        m_frameDataBind->updateValue("ResetPasswordVisible", value);
    });

    m_singleAuth->setKeyboardButtonVisible(m_keyboardList.size() > 1 ? true : false);
    m_singleAuth->setKeyboardButtonInfo(m_keyboardType);
    m_singleAuth->setCapsLockVisible(m_capslockMonitor->isCapslockOn());
    m_singleAuth->setPasswordHint(m_model->currentUser()->passwordHint());
    // m_singleAuth->setAuthStatus(m_frameDataBind->getValue("SFSingleAuthStatus").toInt(),
    //                             m_frameDataBind->getValue("SFSingleAuthMsg").toString());
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
    m_passwordAuth->setShowAuthStatus(false);

    connect(m_passwordAuth, &AuthPassword::activeAuth, this, [this] {
        emit requestStartAuthentication(m_user->name(), AuthTypePassword);
    });
    connect(m_passwordAuth, &AuthPassword::authFinished, this, [this](const int authStatus) {
        if (authStatus == StatusCodeSuccess) {
            m_lastAuth = m_passwordAuth;
            m_lockButton->setEnabled(true);
        }
    });
    connect(m_passwordAuth, &AuthPassword::requestAuthenticate, this, [this] {
        const QString &text = m_passwordAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        m_passwordAuth->setAuthStatusStyle(LOGIN_SPINNER);
        m_passwordAuth->setAnimationStatus(true);
        m_passwordAuth->setLineEditEnabled(false);
        m_lockButton->setEnabled(false);
        emit sendTokenToAuth(m_user->name(), AuthTypePassword, text);
    });
    connect(m_passwordAuth, &AuthPassword::requestShowKeyboardList, this, &SFAWidget::showKeyboardList);
    connect(m_lockButton, &QPushButton::clicked, m_passwordAuth, &AuthPassword::requestAuthenticate);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_passwordAuth, &AuthPassword::setCapsLockVisible);
    /* 输入框数据同步 */
    std::function<void(QVariant)> passwordChanged = std::bind(&SFAWidget::syncPassword, this, std::placeholders::_1);
    registerSyncFunctions("SFPasswordAuth", passwordChanged);
    connect(m_passwordAuth, &AuthPassword::lineEditTextChanged, this, [this](const QString &value) {
        m_frameDataBind->updateValue("SFPasswordAuth", value);
        m_lockButton->setEnabled(!value.isEmpty());
    });
    /* 重置密码可见性数据同步 */
    std::function<void(QVariant)> resetPasswordVisibleChanged = std::bind(&SFAWidget::syncPasswordResetPasswordVisibleChanged, this, std::placeholders::_1);
    registerSyncFunctions("ResetPasswordVisible", resetPasswordVisibleChanged);
    connect(m_passwordAuth, &AuthPassword::resetPasswordMessageVisibleChanged, this, [ = ](const bool value) {
        m_frameDataBind->updateValue("ResetPasswordVisible", value);
    });

    connect(m_passwordAuth, &AuthPassword::focusChanged, this, [this](bool focus) {
        if (!focus) {
            m_keyboardTypeBorder->setVisible(focus);
        }
    });

    m_passwordAuth->setCapsLockVisible(m_capslockMonitor->isCapslockOn());
    m_passwordAuth->setKeyboardButtonInfo(m_user->keyboardLayout());
    m_passwordAuth->setKeyboardButtonVisible(m_user->keyboardLayoutList().size() > 1);
    m_passwordAuth->setPasswordHint(m_user->passwordHint());
    // m_passwordAuth->setAuthStatus(m_frameDataBind->getValue("SFPasswordAuthStatus").toInt(),
    //                               m_frameDataBind->getValue("SFPasswordAuthMsg").toString());

    /* 认证选择按钮 */
    DButtonBoxButton *btn = new DButtonBoxButton(QIcon(Password_Auth), QString(), this);
    btn->setIconSize(QSize(24, 24));
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AuthTypePassword, btn);
    connect(btn, &DButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_passwordAuth);
            m_frameDataBind->updateValue("SFAType", AuthTypePassword);
            emit requestStartAuthentication(m_user->name(), AuthTypePassword);
        } else {
            m_passwordAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AuthTypePassword);
        }
    });
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
    m_fingerprintAuth->setAuthStatusLabel(m_biometricAuthStatus);

    connect(m_fingerprintAuth, &AuthFingerprint::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeFingerprint);
    });
    connect(m_fingerprintAuth, &AuthFingerprint::authFinished, this, [this](const int authStatus) {
        if (authStatus == StatusCodeSuccess) {
            m_lastAuth = m_fingerprintAuth;
            m_lockButton->setEnabled(true);
            m_lockButton->setFocus();
        } else {
            m_lockButton->setEnabled(false);
        }
    });

    // m_fingerprintAuth->setAuthStatus(m_frameDataBind->getValue("SFPasswordAuthStatus").toInt(),
    //                                  m_frameDataBind->getValue("SFPasswordAuthMsg").toString());

    /* 认证选择按钮 */
    DButtonBoxButton *btn = new DButtonBoxButton(QIcon(Fingerprint_Auth), QString(), this);
    btn->setIconSize(QSize(24, 24));
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AuthTypeFingerprint, btn);
    connect(btn, &DButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_fingerprintAuth);
            m_fingerprintAuth->setAuthStatueVisible(true);
            m_frameDataBind->updateValue("SFAType", AuthTypeFingerprint);
            emit requestStartAuthentication(m_user->name(), AuthTypeFingerprint);
        } else {
            m_fingerprintAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AuthTypeFingerprint);
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

    connect(m_ukeyAuth, &AuthUKey::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeUkey);
    });
    connect(m_ukeyAuth, &AuthUKey::authFinished, this, [this](const int authStatus) {
        if (authStatus == StatusCodeSuccess) {
            m_lastAuth = m_ukeyAuth;
            m_lockButton->setEnabled(true);
            m_lockButton->setFocus();
        } else {
            m_lockButton->setEnabled(false);
        }
    });
    connect(m_ukeyAuth, &AuthUKey::requestAuthenticate, this, [=] {
        const QString &text = m_ukeyAuth->lineEditText();
        if (text.isEmpty()) {
            return;
        }
        m_ukeyAuth->setAuthStatusStyle(LOGIN_SPINNER);
        m_ukeyAuth->setAnimationStatus(true);
        m_ukeyAuth->setLineEditEnabled(false);
        m_lockButton->setEnabled(false);
        emit sendTokenToAuth(m_model->currentUser()->name(), AuthTypeUkey, text);
    });
    connect(m_lockButton, &QPushButton::clicked, m_ukeyAuth, &AuthUKey::requestAuthenticate);
    connect(m_capslockMonitor, &KeyboardMonitor::capslockStatusChanged, m_ukeyAuth, &AuthUKey::setCapsLockVisible);
    /* 输入框数据同步 */
    std::function<void(QVariant)> PINChanged = std::bind(&SFAWidget::syncUKey, this, std::placeholders::_1);
    registerSyncFunctions("SFUKeyAuth", PINChanged);
    connect(m_ukeyAuth, &AuthUKey::lineEditTextChanged, this, [this](const QString &value) {
        m_frameDataBind->updateValue("SFUKeyAuth", value);
        if (m_model->getAuthProperty().PINLen > 0 && value.size() >= m_model->getAuthProperty().PINLen) {
            emit m_ukeyAuth->requestAuthenticate();
        }
    });

    m_ukeyAuth->setCapsLockVisible(m_capslockMonitor->isCapslockOn());

    /* 认证选择按钮 */
    DButtonBoxButton *btn = new DButtonBoxButton(QIcon(UKey_Auth), QString(), this);
    btn->setIconSize(QSize(24, 24));
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AuthTypeUkey, btn);
    connect(btn, &DButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_ukeyAuth);
            m_frameDataBind->updateValue("SFAType", AuthTypeUkey);
            emit requestStartAuthentication(m_user->name(), AuthTypeUkey);
        } else {
            m_ukeyAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AuthTypeUkey);
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
    m_faceAuth->setAuthStatusLabel(m_biometricAuthStatus);
    m_faceAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);
    m_faceAuth->hide();

    connect(m_faceAuth, &AuthFace::retryButtonVisibleChanged, this, &SFAWidget::onRetryButtonVisibleChanged);
    connect(m_retryButton, &DFloatingButton::clicked, this, [this] {
        onRetryButtonVisibleChanged(false);
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeFace);
    });
    connect(m_faceAuth, &AuthFace::activeAuth, this, [this] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeFace);
    });
    connect(m_faceAuth, &AuthFace::authFinished, this, [this](const int authStatus) {
        if (authStatus == StatusCodeSuccess) {
            m_lastAuth = m_faceAuth;
            m_lockButton->setEnabled(true);
            m_lockButton->setFocus();
        }
    });
    connect(m_lockButton, &QPushButton::clicked, this, [this] {
        if (m_faceAuth->authStatus() == StatusCodeSuccess) {
            emit authFinished();
        }
    });

    // m_faceAuth->setAuthStatus(m_frameDataBind->getValue("SFFaceAuthStatus").toInt(),
    //                           m_frameDataBind->getValue("SFFaceAuthMsg").toString());

    /* 认证选择按钮 */
    DButtonBoxButton *btn = new DButtonBoxButton(QIcon(Face_Auth), QString(), this);
    btn->setIconSize(QSize(24, 24));
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AuthTypeFace, btn);
    connect(btn, &DButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_faceAuth);
            m_faceAuth->setAuthStatueVisible(true);
            m_frameDataBind->updateValue("SFAType", AuthTypeFace);
            emit requestStartAuthentication(m_user->name(), AuthTypeFace);
        } else {
            m_faceAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AuthTypeFace);
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
    m_irisAuth->setAuthStatusLabel(m_biometricAuthStatus);
    m_irisAuth->setAuthFactorType(DDESESSIONCC::SingleAuthFactor);

    connect(m_irisAuth, &AuthIris::retryButtonVisibleChanged, this, &SFAWidget::onRetryButtonVisibleChanged);
    connect(m_retryButton, &DFloatingButton::clicked, this, [ this ] {
        onRetryButtonVisibleChanged(false);
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeIris);
    });
    connect(m_irisAuth, &AuthIris::activeAuth, this, [ this ] {
        emit requestStartAuthentication(m_model->currentUser()->name(), AuthTypeIris);
    });
    connect(m_irisAuth, &AuthIris::authFinished, this, [ this ] (const int authStatus) {
        if (authStatus == StatusCodeSuccess) {
            m_lastAuth = m_irisAuth;
            m_lockButton->setEnabled(true);
            m_lockButton->setFocus();
        }
    });
    connect(m_lockButton, &QPushButton::clicked, this, [ this ] {
        if (m_irisAuth->authStatus() == StatusCodeSuccess) {
            emit authFinished();
        }
    });

    /* 认证选择按钮 */
    DButtonBoxButton *btn = new DButtonBoxButton(QIcon(Iris_Auth), QString(), this);
    btn->setIconSize(QSize(24, 24));
    btn->setFocusPolicy(Qt::NoFocus);
    m_authButtons.insert(AuthTypeIris, btn);
    connect(btn, &DButtonBoxButton::toggled, this, [this](const bool checked) {
        if (checked) {
            replaceWidget(m_irisAuth);
            m_irisAuth->setAuthStatueVisible(true);
            m_frameDataBind->updateValue("SFAType", AuthTypeIris);
            emit requestStartAuthentication(m_user->name(), AuthTypeIris);
        } else {
            m_irisAuth->hide();
            m_lockButton->setEnabled(false);
            emit requestEndAuthentication(m_user->name(), AuthTypeIris);
        }
    });
}

/**
 * @brief SFAWidget::checkAuthResult
 * @param type
 * @param status
 */
void SFAWidget::checkAuthResult(const int type, const int status)
{
    Q_UNUSED(type)
    Q_UNUSED(status)
}

/**
 * @brief 多屏同步认证类型
 * @param value
 */
void SFAWidget::syncAuthType(const QVariant &value)
{
    m_chooesAuthButtonBox->button(value.toInt())->setChecked(true);
}

void SFAWidget::resizeEvent(QResizeEvent *event)
{
    return AuthWidget::resizeEvent(event);
}


void SFAWidget::replaceWidget(AuthModule *authModule)
{
    if (authModule == m_currentAuth)
        return;

    m_mainLayout->replaceWidget(m_currentAuth, authModule);
    m_currentAuth->hide();
    m_currentAuth->setAuthStatueVisible(false);
    m_currentAuth = authModule;
    authModule->show();
    setFocusProxy(authModule);
    setFocus();
    onRetryButtonVisibleChanged(false);
}

void SFAWidget::onRetryButtonVisibleChanged(bool visible)
{
    m_retryButton->setVisible(visible);
    m_lockButton->setVisible(!visible);
}
