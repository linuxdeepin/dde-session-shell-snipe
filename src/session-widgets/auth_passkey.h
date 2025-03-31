// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHPASSKEY_H
#define AUTHPASSKEY_H

#include "auth_module.h"

#include <QHBoxLayout>

#include <DSpinner>

DWIDGET_USE_NAMESPACE

#define Face_Passkey QStringLiteral(":/misc/images/auth/passkey.svg")

class AuthPasskey : public AuthModule
{
    Q_OBJECT
public:
    explicit AuthPasskey(QWidget *parent = nullptr);

    void reset();

public slots:
    void setAuthState(const AuthCommon::AuthState state, const QString &result) override;
    void setAnimationState(const bool start) override;
    void setLimitsInfo(const LimitsInfo &info) override;
    void setAuthFactorType(AuthFactorType authFactorType) override;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void initUI();
    void initConnections();
    void doAnimation() override;
    void needSpinner(bool need);

signals:
    void retryButtonVisibleChanged(bool visible);

private:
    int m_aniIndex;
    DLabel *m_textLabel;
    DSpinner *m_spinner;
    QHBoxLayout *m_stretchLayout;
};

#endif // AUTHPASSKEY_H
