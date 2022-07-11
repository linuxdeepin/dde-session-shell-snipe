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

#ifndef SFAWIDGET_H
#define SFAWIDGET_H

#include "auth_widget.h"
#include "userinfo.h"
#include "authcommon.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DButtonBox>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLabel>

#include <QWidget>

class AuthModule;
class KbLayoutWidget;
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

    void setModel(const SessionBaseModel *model) override;
    void setAuthType(const int type) override;
    void setAuthState(const int type, const int state, const QString &message) override;
    int getTopSpacing() const override;

public slots:
    void onRetryButtonVisibleChanged(bool visible);
    void onRequestChangeAuth(const int authType);

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void updateBlurEffectGeometry();

private:
    void initUI();
    void initConnections();

    void initSingleAuth();
    void initPasswdAuth();
    void initFingerprintAuth();
    void initUKeyAuth();
    void initFaceAuth();
    void initIrisAuth();
    void initCustomAuth();

    void checkAuthResult(const int type, const int state) override;

    void syncAuthType(const QVariant &value);
    void replaceWidget(AuthModule *authModule);
    void setBioAuthStateVisible(AuthModule *authModule, bool visible);
    void updateSpaceItem();
    void updateFocus();
    void initAccount();

private:
    QVBoxLayout *m_mainLayout;

    DButtonBox *m_chooseAuthButtonBox; // 认证选择按钮
    DLabel *m_biometricAuthState;      // 生物认证状态

    QMap<int, DButtonBoxButton *> m_authButtons;
    DFloatingButton *m_retryButton;
    QSpacerItem *m_bioAuthStatePlaceHolder;
    QSpacerItem *m_bioBottomSpacingHolder;
    QSpacerItem *m_authTypeBottomSpacingHolder;
    AuthCommon::AuthType m_currentAuthType;
};

#endif // SFAWIDGET_H
