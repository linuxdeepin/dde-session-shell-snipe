// SPDX-FileCopyrightText: 2011 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <dtkwidget_global.h>
#include <dtkcore_global.h>
#include "userinfo.h"
#include "tray_plugin.h"

#include <DFloatingButton>
#include <DStyleOptionButton>

#include <QWidget>
#include <QMouseEvent>

namespace dss {
namespace module {
class BaseModuleInterface;
}
} // namespace dss

DWIDGET_USE_NAMESPACE

DWIDGET_BEGIN_NAMESPACE
class DArrowRectangle;
DWIDGET_END_NAMESPACE

class MediaWidget;
class QHBoxLayout;
class QPropertyAnimation;
class QLabel;
class QMenu;
class SessionBaseModel;
class KBLayoutListView;
class TipsWidget;
class RoundPopupWidget;
class SessionPopupWidget;
class TipContentWidget;
class UserListPopupWidget;

const int BlurRadius = 15;
const int BlurTransparency = 70;

class FloatingButton : public DFloatingButton
{
    Q_OBJECT
public:
    enum IconState {
        Normal,
        Hover,
        Press
    };
    explicit FloatingButton(QWidget *parent = nullptr)
        : DFloatingButton(parent) {
        installEventFilter(this);
    }
    explicit FloatingButton(QStyle::StandardPixmap iconType = static_cast<QStyle::StandardPixmap>(-1), QWidget *parent = nullptr)
        : DFloatingButton(iconType, parent) {
        installEventFilter(this);
    }
    explicit FloatingButton(DStyle::StandardPixmap iconType = static_cast<DStyle::StandardPixmap>(-1), QWidget *parent = nullptr)
        : DFloatingButton(iconType, parent) {
        installEventFilter(this);
    }
    explicit FloatingButton(const QString &text, QWidget *parent = nullptr)
        : DFloatingButton(text, parent) {
        installEventFilter(this);
    }
    FloatingButton(const QIcon& icon, const QString &text = QString(), QWidget *parent = nullptr)
        : DFloatingButton(icon, text, parent) {
        installEventFilter(this);
    }

    void setTipText(const QString &tipText) {
        m_tipText = tipText;
    }
    QString tipText() const {
        return m_tipText;
    }

Q_SIGNALS:
    void requestShowMenu();
    void requestShowTips();
    void requestHideTips();
    void buttonHide();

protected:
    bool eventFilter(QObject *watch, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void showCustomWidget() const;

    IconState m_State = Normal;
    QString m_tipText;
};

class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(const SessionBaseModel *model, QWidget *parent = nullptr);
    ~ControlWidget() override;

    void setModel(const SessionBaseModel *model);
    void setUser(std::shared_ptr<User> user);

    void initKeyboardLayoutList();
    QWidget *getTray(const QString &name);
    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

signals:
    void requestSwitchUser(std::shared_ptr<User> user);
    void requestShutdown();
    void requestSwitchSession(const QString &session);
    void requestSwitchVirtualKB();
    void requestKeyboardLayout(const QPoint &pos);
    void requestShowModule(const QString &name, const bool callShowForce = false);
    void notifyKeyboardLayoutHidden();

    void requestTogglePopup(QPoint globalPos, QWidget *popup);
    void requestHidePopup();

public slots:
    void addModule(TrayPlugin *module);
    void removeModule(TrayPlugin *module);
    void setVirtualKBVisible(bool visible);
    void setUserSwitchEnable(const bool visible);
    void setSessionSwitchEnable(const bool visible);
    void chooseToSession(const QString &session);
    void leftKeySwitch();
    void rightKeySwitch();
    void setKBLayoutVisible();
    void setKeyboardType(const QString& str);
    void setKeyboardList(const QStringList& str);
    void onItemClicked(const QString& str);
    void updateModuleVisible();
    void setCanShowMenu(bool state) { m_canShowMenu = state; }
    void updatePluginVisible(const QString module, bool state);

protected:
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *event) override;

private:
    void initUI();
    void initConnect();
    void updateLayout();
    void updateTapOrder();
    static QString messageCallback(const QString &message, void *app_data);
    void toggleButtonPopup(const FloatingButton *button, QWidget *popup);

private slots:
    void showInfoTips();
    void hideInfoTips();
    void showSessionPopup();
    void showUserListPopupWidget();

private:
    int m_index = 0;
    QList<DFloatingButton *> m_btnList;

    QHBoxLayout *m_mainLayout = nullptr;
    FloatingButton *m_switchUserBtn = nullptr;
    FloatingButton *m_powerBtn = nullptr;
    FloatingButton *m_sessionBtn = nullptr;
    FloatingButton *m_virtualKBBtn = nullptr;
    FloatingButton *m_keyboardBtn = nullptr; // 键盘布局按钮

    std::shared_ptr<User> m_curUser;
    const SessionBaseModel *m_model = nullptr;
    QList<QMetaObject::Connection> m_connectionList;

    QMenu *m_contextMenu = nullptr;
    QMap<QPointer<QWidget>, QPointer<QWidget>> m_modules;
    QMap<QString, bool> m_modulesVisible;

    bool m_onboardBtnVisible = true;
    bool m_doGrabKeyboard = true;
    bool m_canShowMenu = true;

    TipContentWidget *m_tipContentWidget = nullptr;       // 显示按钮文字tip
    TipsWidget *m_tipsWidget = nullptr;                   // 显示插件提供widget tip

    RoundPopupWidget *m_roundPopupWidget = nullptr;       // 圆角弹窗
    KBLayoutListView *m_kbLayoutListView = nullptr;       // 键盘布局列表
    SessionPopupWidget *m_sessionPopupWidget = nullptr;   // session 列表
    UserListPopupWidget *m_userListPopupWidget = nullptr; // 用户列表
};

#endif // CONTROLWIDGET_H
