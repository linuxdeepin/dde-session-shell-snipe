// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fullmanagedauthwidget.h"

#include "auth_custom.h"
#include "authcommon.h"
#include "dlineeditex.h"
#include "keyboardmonitor.h"
#include "modules_loader.h"
#include "sessionbasemodel.h"
#include "useravatar.h"
#include "plugin_manager.h"

QList<FullManagedAuthWidget *> FullManagedAuthWidget::FullManagedAuthWidgetObjs = {};
FullManagedAuthWidget::FullManagedAuthWidget(QWidget *parent)
    : AuthWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_currentAuthType(AT_All)
    , m_inited(false)
    , m_isPluginLoaded(false)
{
    setObjectName(QStringLiteral("FullManagedAuthWidget"));
    setAccessibleName(QStringLiteral("FullManagedAuthWidget"));

    FullManagedAuthWidgetObjs.append(this);
}

FullManagedAuthWidget::~FullManagedAuthWidget()
{
    FullManagedAuthWidgetObjs.removeAll(this);
}

void FullManagedAuthWidget::hideInternalControls()
{
    auto hide = [](QWidget *wid) {
        if (wid) {
            wid->setVisible(false);
            wid->setDisabled(true);
        }
    };

    hide(m_accountEdit);
    hide(m_userNameWidget);
    hide(m_userAvatar);
    hide(m_lockButton);
}

void FullManagedAuthWidget::initUI()
{
    AuthWidget::initUI();
}

void FullManagedAuthWidget::initConnections()
{
    AuthWidget::initConnections();
    connect(m_model, &SessionBaseModel::authTypeChanged, this, &FullManagedAuthWidget::setAuthType);
    connect(m_model, &SessionBaseModel::authStateChanged, this, &FullManagedAuthWidget::setAuthState);
}

void FullManagedAuthWidget::setModel(const SessionBaseModel *model)
{
    AuthWidget::setModel(model);

    initUI();
    initConnections();

    // 在登陆设置验证类型的时候需要判断当前是否是"..."账户，需要先设置当前用户，在设置验证类型，两者的顺序切勿颠倒。
    setUser(model->currentUser());
    setAuthType(model->getAuthProperty().AuthType);
    hideInternalControls();

    m_inited = true;
}

void FullManagedAuthWidget::setAuthType(const AuthFlags type)
{
    int authType = type;
    LoginPlugin *plugin = PluginManager::instance()->getFullManagedLoginPlugin();
    if (plugin
        && !m_model->currentUser()->isNoPasswordLogin()
        && !m_model->currentUser()->isAutomaticLogin()) {
        authType |= AT_Custom;
        initCustomAuth();

        // 只有当首次创建sfa或者这个对象已经初始化过了才应用DefaultAuthLevel
        // 这是只是一个规避方案，主要是因为每个屏幕都会创建一个sfa，这看起来不太合理，特别是处理单例对象时，带来很大的不便。

        // TODO 每个屏幕只创建一个LockContent，鼠标在屏幕间移动的时候重新设置LockContent的parent即可。
        qCInfo(DDE_SHELL) << "FMA is inited: " << m_inited << ", FMA widgets size: " << FullManagedAuthWidgetObjs.size();
        if (m_inited || FullManagedAuthWidgetObjs.size() <= 1) {
            if (m_customAuth->pluginConfig().defaultAuthLevel == LoginPlugin::DefaultAuthLevel::Default) {
                m_currentAuthType = m_currentAuthType == AT_All ? AT_Custom : m_currentAuthType;
            } else if (m_customAuth->pluginConfig().defaultAuthLevel == LoginPlugin::DefaultAuthLevel::StrongDefault) {
                m_currentAuthType = AT_Custom;
            }
        }
    } else if (m_customAuth) {
        delete m_customAuth;
        m_customAuth = nullptr;
    }

    if (m_inited && m_customAuth)
        Q_EMIT requestStartAuthentication(m_model->currentUser()->name(), AT_Custom);
}

void FullManagedAuthWidget::setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message)
{
    qCDebug(DDE_SHELL) << "Set auth state, type: " << type << ", state: " << state << ", message: " << message;
    if (!isPluginLoaded()) {
        qCDebug(DDE_SHELL) << "Plugin not load or actived";
        return;
    }

    if (!m_customAuth) {
        qCDebug(DDE_SHELL) << Q_FUNC_INFO << "custom auth not init";
        return;
    }

    switch (type) {
    case AT_PAM:
    case AT_Custom:
        // 根据早期设计,自定义的插件需要通过lightdmAuthStarted方法通知插件,然后插件send token回来
        // 插件侧需要处理命名为"StartAuth"的交互请求
        if (state == AS_Prompt && m_model->appType() == AuthCommon::AppType::Login) {
            m_customAuth->lightdmAuthStarted();
            break;
        }

        m_customAuth->setAuthState(state, message);
        break;
    case AT_All:
        checkAuthResult(AT_All, state);
        break;
    default:
        break;
    }

    // 同步验证状态给插件
    if (m_customAuth) {
        m_customAuth->notifyAuthState(type, state);
    }
}

void FullManagedAuthWidget::initCustomAuth()
{
    qCDebug(DDE_SHELL) << "Init custom auth";
    if (m_customAuth) {
        return;
    }

    auto plugin = PluginManager::instance()->getFullManagedLoginPlugin();
    if (!plugin) {
        qCDebug(DDE_SHELL) << "No full managed plugin found";
        return;
    }

    m_customAuth = new AuthCustom(this);
    m_customAuth->setModule(plugin);
    m_customAuth->setModel(m_model);
    m_customAuth->initUi();
    m_customAuth->show();
    m_mainLayout->addWidget(m_customAuth);

    setLayout(m_mainLayout);

    connect(m_customAuth, &AuthCustom::requestSendToken, this, [this](const QString &token) {
        qCInfo(DDE_SHELL) << "Fma custom send token name :" << m_user->name();
        Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, token);
    });

    connect(m_customAuth, &AuthCustom::requestCheckAccount, this, [this](const QString &account) {
        qCInfo(DDE_SHELL) << "Request check account: " << account;

        if (account.isEmpty()) {
            qCInfo(DDE_SHELL) << "Account is empty" << account;
            return;
        }


        // 用户不一致时切换用户
        if (m_user->name() != account) {
            // 切换用户
            Q_EMIT requestCheckAccount(account);
        } else {
            qCDebug(DDE_SHELL) << "Start to send token";
            LoginPlugin::AuthCallbackData data = m_customAuth->getCurrentAuthData();
            if (data.result == LoginPlugin::AuthResult::Success) {
                Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, data.token);
            } else {
                qCWarning(DDE_SHELL) << "Token auth failed , result: " << data.result;
            }
        }
    });

    connect(m_customAuth, &AuthCustom::notifyAuthTypeChange, this, &FullManagedAuthWidget::onRequestChangeAuth);
    m_isPluginLoaded = true;
}

void FullManagedAuthWidget::checkAuthResult(const AuthType type, const AuthState state)
{
    // 等所有类型验证通过的时候在发送验证完成信息，否则DA的验证结果可能还没有刷新，导致lightdm调用pam验证失败
    // 人脸和虹膜是手动点击解锁按钮后发送，无需处理
    if (type == AT_All && state == AS_Success) {
        if (m_customAuth && m_customAuth->authState() == AS_Success) {
            emit authFinished();
        }

        if (m_customAuth) {
            m_customAuth->resetAuth();
            m_customAuth->reset();
        }
    }
}

void FullManagedAuthWidget::resizeEvent(QResizeEvent *event)
{
    updateBlurEffectGeometry();

    AuthWidget::resizeEvent(event);
}

void FullManagedAuthWidget::showEvent(QShowEvent *event)
{
    // fullmanaged plugin start auth on UI button clicked
    Q_EMIT requestStartAuthentication(m_user->name(), AT_Custom);
    AuthWidget::showEvent(event);
}

// plugin do not neet to response auth changing
void FullManagedAuthWidget::onRequestChangeAuth(const AuthType authType)
{
    Q_UNUSED(authType);
}

bool FullManagedAuthWidget::isPluginLoaded() const
{
    if (!m_customAuth) {
        return false;
    }

    auto plugin = m_customAuth->getLoginPlugin();
    if (!plugin) {
        return false;
    }

    return m_isPluginLoaded && plugin->isPluginEnabled();
}

bool FullManagedAuthWidget::isUserSwitchButtonVisiable() const
{
    // get config from plugin
    do {
        if(!isPluginLoaded()){
            break;
        }

        auto plugin = m_customAuth->getLoginPlugin();
        if (!plugin) {
            break;
        }

        return plugin->pluginConfig().showSwitchButton;
    } while(false);

    return PluginConfig::isUserSwitchButtonVisiable();
}
