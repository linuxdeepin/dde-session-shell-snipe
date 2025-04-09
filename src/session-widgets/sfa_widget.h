// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SFAWIDGET_H
#define SFAWIDGET_H

#include "auth_widget.h"
#include "userinfo.h"
#include "authcommon.h"
#include "buttonbox.h"
#include "assist_login_widget.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DButtonBox>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLabel>

#include <QWidget>

class AuthModule;
class KeyboardMonitor;

class DLineEditEx;
class QVBoxLayout;
class UserAvatar;
class QSpacerItem;

class SFAWidget : public AuthWidget
{
    Q_OBJECT

public:
    explicit SFAWidget(QWidget *parent = nullptr);
    ~SFAWidget();

    void setModel(const SessionBaseModel *model) override;
    void setAuthType(const AuthCommon::AuthFlags type) override;
    void setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message) override;
    int getTopSpacing() const override;

public slots:
    void onRetryButtonVisibleChanged(bool visible);
    void onRequestChangeAuth(const AuthCommon::AuthType authType);
    void onLightdmPamStartChanged();

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void updateBlurEffectGeometry();
    void onActiveAuth(AuthCommon::AuthType authType);
    void onAccountError();

private:
    void initUI();
    void initConnections();
    void initAuthFactors(const AuthCommon::AuthFlags type);
    void initCustomFactors(const AuthCommon::AuthFlags type);
    void chooseAuthType(const AuthCommon::AuthFlags authFlags);
    void initChooseAuthButtonBox(const AuthCommon::AuthFlags authFlags);

    void initSingleAuth();
    void initPasswdAuth();
    void initFingerprintAuth();
    void initPasskeyAuth();
    void initUKeyAuth();
    void initFaceAuth();
    void initIrisAuth();
    void initCustomAuth(LoginPlugin* plugin);
    void initCustomMFAAuth();

    void checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state) override;

    void replaceWidget(AuthModule *authModule);
    void setBioAuthStateVisible(AuthModule *authModule, bool visible);
    void updateSpaceItem();
    void updateFocus();
    void initAccount();
    bool useCustomAuth() const;
    bool showAuthButtonBox() const;
    void sendAuthFinished();
    QList<LoginPlugin*> filtrateAuthPlugins(const QList<LoginPlugin*> &plugins) const;
    AuthCustom *currentAuthCustom() const;
    void removeAuthButton(AuthCustom *auth);

private:
    QVBoxLayout *m_mainLayout;

    ButtonBox *m_chooseAuthButtonBox; // 认证选择按钮
    DLabel *m_biometricAuthState;      // 生物认证状态

    DFloatingButton *m_retryButton;
    QSpacerItem *m_bioAuthStatePlaceHolder;
    QSpacerItem *m_bioBottomSpacingHolder;
    QSpacerItem *m_authTypeBottomSpacingHolder;
    int m_currentAuthType;

    QMap<int, ButtonBoxButton *> m_authButtons;
    QMap<QString, QPointer<AuthCustom>> m_customAuths;
};

#endif // SFAWIDGET_H
