// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHWIDGET_H
#define AUTHWIDGET_H

#include "userinfo.h"
#include "user_name_widget.h"
#include "transparentbutton.h"
#include "authcommon.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLabel>
#include <DStyleOptionButton>
#include <DSingleton>

#include <QWidget>
#include <QMap>
#include <QPair>
#include <QPointer>

class AuthSingle;
class AuthIris;
class AuthFace;
class AuthUKey;
class AuthFingerprint;
class AuthPassword;
class AuthCustom;
class AuthPasskey;
class DLineEditEx;
class KeyboardMonitor;
class SessionBaseModel;
class UserAvatar;

DWIDGET_USE_NAMESPACE

// 把widget和SpacerItem进行绑定，widget隐藏的时候sapcerItem也隐藏，避免有多余的空白导致布局不符合设计
class SpacerItemBinder : public QObject, public Dtk::Core::DSingleton<SpacerItemBinder>
{
    Q_OBJECT

    friend class Dtk::Core::DSingleton<SpacerItemBinder>;
public:

    void bind(QWidget* w, QSpacerItem* item, QSize size)
    {
        if (!w || !item) {
            return;
        }

        connect(w, &QObject::destroyed, this, [w, this] {
            m_bindingMap.remove(w);
        });

        auto s = w->isVisible() ? size : QSize(0, 0);
        item->changeSize(s.width(), s.height());
        w->installEventFilter(this);

        m_bindingMap.insert(w, qMakePair(item, size));
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ((event->type() == QEvent::Hide || event->type() == QEvent::Show)) {
            auto w = qobject_cast<QWidget*>(watched);
            if (m_bindingMap.contains(w)) {
                auto pair = m_bindingMap.value(w);
                if (pair.first) {
                    auto size = event->type() == QEvent::Hide ? QSize(0, 0) : pair.second;
                    pair.first->changeSize(size.width(), size.height());
                    Q_EMIT requestInvalidateLayout();
                }
            }
        }

        return false;
    }

    static void addWidget(QWidget* w, QBoxLayout* layout, Qt::Alignment alignment = Qt::AlignVCenter, int h = 10)
    {
        QSpacerItem* item = new QSpacerItem(0, h);
#if DTK_VERSION < DTK_VERSION_CHECK(6, 0, 0, 0)
        SpacerItemBinder::instance()->bind(w, item, QSize(0, h));
#else
        SpacerItemBinder::ref().bind(w, item, QSize(0, h));
#endif
        layout->addWidget(w, 0, alignment);
        layout->addSpacerItem(item);
    }

    void changeItemSize(QWidget* w, QSize size)
    {
        if (!m_bindingMap.contains(w)) {
            return;
        }

        auto pair = m_bindingMap.value(w);
        if (pair.first) {
            pair.first->changeSize(size.width(), size.height());
            pair.second = size;
            m_bindingMap[w] = pair;
        }
        Q_EMIT requestInvalidateLayout();
    }

Q_SIGNALS:
    void requestInvalidateLayout();

private:
    SpacerItemBinder() = default;
    ~SpacerItemBinder() = default;

private:
    QMap<QWidget*, QPair<QSpacerItem*, QSize>> m_bindingMap;
};

class AuthWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AuthWidget(QWidget *parent = nullptr);
    ~AuthWidget() override;

    virtual void setModel(const SessionBaseModel *model);
    virtual void setAuthType(const AuthCommon::AuthFlags type);
    virtual void setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message);
    virtual int getTopSpacing() const;
    virtual QWidget* getAuthWidget();

    void setAccountErrorMsg(const QString &message);
    void syncPasswordResetPasswordVisibleChanged(const QVariant &value);
    void syncResetPasswordUI();
    void syncPasswordErrorTipsClearChanged(const QVariant &value);
    void updatePasswordErrortipUi();

Q_SIGNALS:
    void requestCheckSameNameAccount(const QString &account, bool switchUser = true);
    void requestCheckAccount(const QString &account, bool switchUser = true);
    void requestSetKeyboardType(const QString &key);
    void requestStartAuthentication(const QString &account, const AuthCommon::AuthFlags authType);
    void sendTokenToAuth(const QString &account, const AuthCommon::AuthType authType, const QString &token);
    void requestEndAuthentication(const QString &account, const AuthCommon::AuthFlags authType);
    void authFinished();
    void updateParentLayout();
    void noPasswordLoginChanged(const QString &account, bool noPassword);

protected:
    void showEvent(QShowEvent *event) override;

protected:
    void initUI();
    void initConnections();
    virtual void checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state);
    void setUser(std::shared_ptr<User> user);
    void setLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo);
    void setAvatar(const QString &avatar);
    void updateUserDisplayNameLabel();
    void setPasswordHint(const QString &hint);
    void setLockButtonType(const int type);
    void updatePasswordExpiredState();
    void registerSyncFunctions(const QString &flag, std::function<void(QVariant)> function);
    void syncSingle(const QVariant &value);
    void syncSingleResetPasswordVisibleChanged(const QVariant &value);
    void syncAccount(const QVariant &value);
    void syncPassword(const QVariant &value);
    void syncUKey(const QVariant &value);
    int calcCurrentHeight(const int height) const;
    void terminalLockedChanged(bool locked);

protected Q_SLOTS:
    void updateBlurEffectGeometry();
    void onNoPasswordLoginChanged(bool noPassword);

protected:
    const SessionBaseModel *m_model;

    DBlurEffectWidget *m_blurEffectWidget; // 模糊背景
    TransparentButton *m_lockButton;         // 解锁按钮
    UserAvatar *m_userAvatar;              // 用户头像

    DLabel *m_expiredStateLabel;           // 密码过期提示
    DLineEditEx *m_accountEdit;   // 用户名输入框
    UserNameWidget *m_userNameWidget;    // 用户名

    QPointer<AuthSingle> m_singleAuth;           // PAM
    QPointer<AuthPassword> m_passwordAuth;       // 密码
    QPointer<AuthFingerprint> m_fingerprintAuth; // 指纹
    QPointer<AuthUKey> m_ukeyAuth;               // UKey
    QPointer<AuthFace> m_faceAuth;               // 面容
    QPointer<AuthIris> m_irisAuth;               // 虹膜
    QPointer<AuthPasskey> m_passkeyAuth;         // 安全密钥
    QPointer<AuthCustom> m_gestureAuth;          // 手势
    QPointer<AuthCustom> m_customAuth;           // 自定义认证

    QString m_passwordHint;     // 密码提示
    QString m_keyboardType;     // 键盘布局类型
    QStringList m_keyboardList; // 键盘布局列表

    std::shared_ptr<User> m_user; // 当前用户

    QList<QMetaObject::Connection> m_connectionList;
    QMap<QString, int> m_registerFunctions;

    QTimer *m_refreshTimer;
    AuthCommon::AuthState m_authState;  // 当前验证状态
};

#endif // AUTHWIDGET_H
