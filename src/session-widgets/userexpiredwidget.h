/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmail.com>
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

#ifndef USEREXPIREDWIDGET_H
#define USEREXPIREDWIDGET_H

#include "sessionbasemodel.h"

#include <darrowrectangle.h>
#include <DBlurEffectWidget>
#include <DFloatingButton>
#include <DLineEdit>
#include <DLabel>

DWIDGET_USE_NAMESPACE

class User;
class DPasswordEditEx;
class LoginButton;
class FrameDataBind;
class QVBoxLayout;
class QNetworkReply;
class QNetworkAccessManager;

class UserExpiredWidget : public QWidget
{
    Q_OBJECT
public:
    enum AvatarSize {
        AvatarSmallSize = 80,
        AvatarNormalSize = 90,
        AvatarLargeSize = 100
    };

    explicit UserExpiredWidget(QWidget *parent = nullptr);
    ~UserExpiredWidget() override;
    void resetAllState();
    void setFaildTipMessage(const QString &message);
    void setDisplayName(const QString &name);
    void setUserName(const QString &name);
    void setPassword(const QString &passwd);

    void onConfirmClicked();

    void onPasswordChecked(QNetworkReply *reply);
signals:
    void changePasswordFinished();

public slots:
    void updateAuthType(SessionBaseModel::AuthType type);
    void refreshBlurEffectPosition();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initUI();
    void initConnect();
    void onOtherPageConfirmPasswordChanged(const QVariant &value);
    void onOtherPagePasswordChanged(const QVariant &value);
    void onOtherPageOldPasswordChanged(const QVariant &value);
    bool errorFilter(const QString &new_pass, const QString &confirm);
    void updateNameLabel();

private:
    DBlurEffectWidget *m_blurEffectWidget;         //阴影窗体
    QLabel *m_nameLbl;                             //用户名
    SessionBaseModel::AuthType m_authType;         //认证类型
    DLabel *m_expiredTips;                         //提示信息
    DPasswordEditEx *m_passwordEdit;                     //新密码输入框
    DPasswordEditEx *m_confirmPasswordEdit;              //新密码确认
    DFloatingButton *m_lockButton;                 //解锁按钮
    QVBoxLayout *m_userLayout;                     //用户输入框布局
    QVBoxLayout *m_lockLayout;                     //解锁按钮布局
    QStringList m_KBLayoutList;
    QHBoxLayout *m_nameLayout;
    QFrame *m_nameFrame;
    QString m_showName;
    QString m_userName;
    QString m_password;

    QString m_machine_id;                          //唯一机器码
    QString m_requestPrefix;                        //urs
    QString m_newPasswd;

    QNetworkAccessManager *m_modifyPasswordManager;
};

#endif // USEREXPIREDWIDGET_H
