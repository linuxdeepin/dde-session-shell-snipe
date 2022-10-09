// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fullmanagedauthwidget.h"
#include "managed_login_module_interface.h"

#include "auth_custom.h"
#include "authcommon.h"
#include "dlineeditex.h"
#include "framedatabind.h"
#include "keyboardmonitor.h"
#include "modules_loader.h"
#include "sessionbasemodel.h"
#include "useravatar.h"

QList<FullManagedAuthWidget *> FullManagedAuthWidget::FullManagedAuthWidgetObjs = {};
FullManagedAuthWidget::FullManagedAuthWidget(QWidget *parent)
    : AuthWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_currentAuthType(AuthCommon::AuthType::AT_All)
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
    if (m_frameDataBind) {
        m_frameDataBind->clearValue("SFCustomAuthStatus");
        m_frameDataBind->clearValue("SFCustomAuthMsg");
    }
}

void FullManagedAuthWidget::initUI()
{
    AuthWidget::initUI();
    m_frameDataBind->refreshData("SFAAccount");
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

    // do not need this controls in this widget for full managed plugin
    m_accountEdit->setVisible(false);
    m_nameLabel->setVisible(false); // be careful, label will be reset to show when setUser invoked
    m_userAvatar->setVisible(false);
    m_nameLabel->setVisible(false);
    m_lockButton->setVisible(false);

    m_inited = true;
}

void FullManagedAuthWidget::setAuthType(const int type)
{
    qDebug() << Q_FUNC_INFO << "SFAWidget::setAuthType:" << type;
    int authType = type;
    int pluginCount = dss::module::ModulesLoader::instance().findModulesByType(dss::module::BaseModuleInterface::FullManagedLoginType).size();
    if (pluginCount
        && !m_model->currentUser()->isNoPasswordLogin()
        && !m_model->currentUser()->isAutomaticLogin()) {
        authType |= AT_Custom;
        initCustomAuth();

        // 只有当首次创建sfa或者这个对象已经初始化过了才应用DefaultAuthLevel
        // 这是只是一个规避方案，主要是因为每个屏幕都会创建一个sfa，这看起来不太合理，特别是处理单例对象时，带来很大的不便。

        // TODO 每个屏幕只创建一个LockContent，鼠标在屏幕间移动的时候重新设置LockContent的parent即可。
        qInfo() << "FMA is inited: " << m_inited << ", FMA widgets size: " << FullManagedAuthWidgetObjs.size();
        if (m_inited || FullManagedAuthWidgetObjs.size() <= 1) {
            qInfo() << Q_FUNC_INFO << "m_customAuth->authType()" << m_customAuth->authType();
            if (m_customAuth->defaultAuthLevel() == AuthCommon::DefaultAuthLevel::Default) {
                m_currentAuthType = m_currentAuthType == AT_All ? AT_Custom : m_currentAuthType;
            } else if (m_customAuth->defaultAuthLevel() == AuthCommon::DefaultAuthLevel::StrongDefault) {
                if (m_currentAuthType == AT_All || m_currentAuthType == AT_Custom) {
                    m_currentAuthType = m_customAuth->authType();
                }
            }
        }
    } else if (m_customAuth) {
        delete m_customAuth;
        m_customAuth = nullptr;
        m_frameDataBind->clearValue("SFCustomAuthStatus");
        m_frameDataBind->clearValue("SFCustomAuthMsg");
    }
}

void FullManagedAuthWidget::setAuthState(const int type, const int state, const QString &message)
{
    qDebug() << "SFAWidget::setAuthState:" << type << state << message;
    switch (type) {
    case AT_PAM:
    case AT_Password:
    case AT_Custom:
        // 这里是为了让自定义登陆知道ligthdm已经开启验证了
        qInfo() << "Current auth type is: " << m_currentAuthType;
        if (m_customAuth && state == AS_Prompt && m_currentAuthType == AT_Custom) {
            // 有可能DA发送了验证开始，但是lightdm的验证还未开始，此时发送token的话lightdm无法验证通过。
            // ligthdm的pam发送prompt后则认为lightdm的pam已经开启验证
            qInfo() << "Greeter is in authentication";
            m_customAuth->lightdmAuthStarted();
        }

        if (m_customAuth) {
            m_customAuth->setAuthState(state, message);
        }
        break;
    case AT_All:
        checkAuthResult(AT_All, state);
        break;
    default:
        break;
    }

    // 同步验证状态给插件
    if (m_customAuth) {
        m_customAuth->notifyAuthState(static_cast<AuthCommon::AuthType>(type), static_cast<AuthCommon::AuthState>(state));
    }
}

void FullManagedAuthWidget::initCustomAuth()
{
    qDebug() << Q_FUNC_INFO << "init custom auth";
    if (m_customAuth) {
        return;
    }

    auto modules = dss::module::ModulesLoader::instance()
                       .findModulesByType(dss::module::BaseModuleInterface::ModuleType::FullManagedLoginType)
                       .values();
    if (!modules.size()) {
        return;
    }

    dss::module::ManagedLoginModuleInterface *module = dynamic_cast<dss::module::ManagedLoginModuleInterface *>(modules.first());

    m_customAuth = new AuthCustom(this);
    m_customAuth->setModule(module);
    m_customAuth->setModel(m_model);
    m_customAuth->initUi();
    m_customAuth->show();
    m_mainLayout->addWidget(m_customAuth);
    setLayout(m_mainLayout);
    m_frameDataBind->updateValue("SFAType", AT_Custom);

    connect(m_customAuth, &AuthCustom::requestSendToken, this, [this](const QString &token) {
        qInfo() << "fma custom sendToken name :" << m_user->name() << "token:" << token;
        m_frameDataBind->updateValue(QStringLiteral("Account"), m_user->name());
        Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, token);
    });
    connect(m_customAuth, &AuthCustom::requestCheckAccount, this, [this](const QString &account) {
        qInfo() << "Request check account: " << account;
        Q_EMIT requestEndAuthentication(m_user->name(), AT_Custom);
        if (m_user && m_user->name() == account) {
            m_frameDataBind->updateValue(QStringLiteral("Account"), account);
            dss::module::AuthCallbackData data = m_customAuth->getCurrentAuthData();
            if (data.result == dss::module::Success) {
                Q_EMIT requestStartAuthentication(m_user->name(), AT_Custom);
                Q_EMIT sendTokenToAuth(m_user->name(), AT_Custom, QString::fromStdString(data.token));
            } else {
                qWarning() << Q_FUNC_INFO << "auth failed";
            }
            return;
        }

        Q_EMIT requestCheckAccount(account);
    });

    connect(m_customAuth, &AuthCustom::notifyAuthTypeChange, this, &FullManagedAuthWidget::onRequestChangeAuth);
    m_isPluginLoaded = true;
}

void FullManagedAuthWidget::checkAuthResult(const int type, const int state)
{
    // 等所有类型验证通过的时候在发送验证完成信息，否则DA的验证结果可能还没有刷新，导致lightdm调用pam验证失败
    // 人脸和虹膜是手动点击解锁按钮后发送，无需处理
    if (type == AT_All && state == AS_Success) {
        if (m_customAuth && m_customAuth->authState() == AS_Success) {
            emit authFinished();
        }

        if (m_customAuth) {
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
    Q_EMIT requestStartAuthentication(m_user->name(), AT_Custom);
    AuthWidget::showEvent(event);
}

// plugin do not neet to response auth changing
void FullManagedAuthWidget::onRequestChangeAuth(const int authType)
{
    Q_UNUSED(authType);
    //    qInfo() << Q_FUNC_INFO << "authType" << authType << "m_chooseAuthButtonBox->isEnabled()" << m_chooseAuthButtonBox->isEnabled()
    //            << "m_currentAuthType" << m_currentAuthType;

    //    //当在S3/S4阶段时，会获取前一次认证的认证类型（不是AT_Custom），这时认证失败，也需要跳转到指纹认证
    //    if ((m_currentAuthType != AT_Custom && m_model->appType() == AuthCommon::AppType::Login)) {
    //        return;
    //    }

    //    if (!m_authButtons.contains(authType)) {
    //        qDebug() << "onRequestChangeAuth no contain";
    //        m_chooseAuthButtonBox->button(m_authButtons.firstKey())->toggled(true);
    //        return;
    //    }

    //    QAbstractButton *btn = m_chooseAuthButtonBox->button(authType);
    //    if (btn)
    //        emit btn->toggled(true);
}
