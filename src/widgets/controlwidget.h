// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <dtkwidget_global.h>
#include <dtkcore_global.h>
#include "userinfo.h"
#include "tray_plugin.h"

#include <DFloatingButton>
#include <DBlurEffectWidget>
#include <DStyleOptionButton>

#include <QWidget>
#include <QEvent>
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

private:
    IconState m_State = Normal;

Q_SIGNALS:
    void requestShowMenu();
    void requestShowTips();
    void requestHideTips();
    void buttonHide();

protected:
    bool eventFilter(QObject *watch, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
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
    void requestSwitchUser();
    void requestShutdown();
    void requestSwitchSession();
    void requestSwitchVirtualKB();
    void requestKeyboardLayout(const QPoint &pos);
    void requestShowModule(const QString &name, const bool callShowForce = false);
    void notifyKeyboardLayoutHidden();

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

private:
    void initUI();
    void initConnect();
    void showTips();
    void hideTips();
    void updateLayout();
    void updateTapOrder();
    static QString messageCallback(const QString &message, void *app_data);

private:
    int m_index = 0;
    QList<DFloatingButton *> m_btnList;

    QHBoxLayout *m_mainLayout = nullptr;
    FloatingButton *m_virtualKBBtn = nullptr;
    FloatingButton *m_switchUserBtn = nullptr;
    FloatingButton *m_powerBtn = nullptr;
    FloatingButton *m_sessionBtn = nullptr;
    QLabel *m_sessionTip = nullptr;
    QWidget *m_tipWidget = nullptr;
#ifndef SHENWEI_PLATFORM
    QPropertyAnimation *m_tipsAni = nullptr;
#endif
    QMap<QPointer<QWidget>, QPointer<QWidget>> m_modules;
    QMap<QString, bool> m_modulesVisible;

    QMenu *m_contextMenu;
    TipsWidget *m_tipsWidget;
    const SessionBaseModel *m_model;

    DArrowRectangle *m_arrowRectWidget;
    KBLayoutListView *m_kbLayoutListView;   // 键盘布局列表
    DFloatingButton *m_keyboardBtn;         // 键盘布局按钮
    std::shared_ptr<User> m_curUser;
    QList<QMetaObject::Connection> m_connectionList;
    bool m_onboardBtnVisible;
    bool m_doGrabKeyboard;
    bool m_canShowMenu;
};

#endif // CONTROLWIDGET_H
