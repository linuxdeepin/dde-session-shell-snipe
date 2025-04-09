// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIGHTERGREETER_H
#define LIGHTERGREETER_H

#include <QWidget>

#include <DLabel>

class QLineEdit;
class QVBoxLayout;
class QListView;
class QComboBox;
class QPushButton;
class QLabel;

namespace QLightDM {
class Greeter;
class SessionsModel;
class UsersModel;
}
DWIDGET_BEGIN_NAMESPACE
class DBlurEffectWidget;
DWIDGET_END_NAMESPACE

class DLineEditEx;
class TransparentButton;
class UserAvatar;

DWIDGET_USE_NAMESPACE

class LighterGreeter : public QWidget
{
    Q_OBJECT
public:
    explicit LighterGreeter(QWidget *parent = nullptr);

    void initUI();
    void initConnections();

Q_SIGNALS:
    void authFinished();

public Q_SLOTS:
    void onStartAuthentication();
    void onAuthenticationComplete();
    void onShowPrompt(QString promptText, int promptType);
    void onShowMessage(QString messageText, int messageType);
    void resetToNormalGreeter();

private Q_SLOTS:
    void respond();
    void updateFocus();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // 布局控件
    QVBoxLayout *m_mainLayout;
    DBlurEffectWidget *m_loginFrame;
    UserAvatar *m_avatar;
    QComboBox *m_userCbx;
    DLineEditEx *m_passwordEdit;
    QComboBox *m_sessionCbx;
    TransparentButton *m_loginBtn;
    QPushButton *m_switchGreeter;

    // greeter
    QLightDM::Greeter *m_greeter;
    QLightDM::SessionsModel *m_sessionModel;

    // variant
    bool m_allowSwitchToWayland;
    QString m_defaultSessionName;
};

#endif // LIGHTERGREETER_H
