// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MFAWIDGET_H
#define MFAWIDGET_H

#include "auth_widget.h"

#include <QWidget>

class MFAWidget : public AuthWidget
{
    Q_OBJECT

public:
    explicit MFAWidget(QWidget *parent = nullptr);

    void setModel(const SessionBaseModel *model) override;
    void setAuthType(const AuthCommon::AuthFlags type) override;
    void setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message) override;
    void autoUnlock();
    int getTopSpacing() const override;

    QWidget *getAuthWidget() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();
    void initConnections();

    void initPasswdAuth();
    void initFingerprintAuth();
    void initUKeyAuth();
    void initFaceAuth();
    void initIrisAuth();
    void initGestureAuth();

    void checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state) override;

    void updateFocusPosition();

private:
    int m_index;
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_authLayout;
    QList<QWidget *> m_widgetList;
};

#endif // MFAWIDGET_H
