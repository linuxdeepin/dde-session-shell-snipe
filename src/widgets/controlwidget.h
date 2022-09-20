// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <dtkwidget_global.h>
#include <dtkcore_global.h>
#include "userinfo.h"

#include <DFloatingButton>
#include <DBlurEffectWidget>

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

DCORE_BEGIN_NAMESPACE
class DConfig;
DCORE_END_NAMESPACE

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

class FlotingButton : public DFloatingButton
{
    Q_OBJECT
public:
    explicit FlotingButton(QWidget *parent = nullptr)
        : DFloatingButton(parent) {
        installEventFilter(this);
    }
    explicit FlotingButton(QStyle::StandardPixmap iconType = static_cast<QStyle::StandardPixmap>(-1), QWidget *parent = nullptr)
        : DFloatingButton(iconType, parent) {
        installEventFilter(this);
    }
    explicit FlotingButton(DStyle::StandardPixmap iconType = static_cast<DStyle::StandardPixmap>(-1), QWidget *parent = nullptr)
        : DFloatingButton(iconType, parent) {
        installEventFilter(this);
    }
    explicit FlotingButton(const QString &text, QWidget *parent = nullptr)
        : DFloatingButton(text, parent) {
        installEventFilter(this);
    }
    FlotingButton(const QIcon& icon, const QString &text = QString(), QWidget *parent = nullptr)
        : DFloatingButton(icon, text, parent) {
        installEventFilter(this);
    }

Q_SIGNALS:
    void requestShowMenu();
    void requestShowTips();
    void requestHideTips();

protected:
    bool eventFilter(QObject *watch, QEvent *event) override;
};
class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(const SessionBaseModel *model, QWidget *parent = nullptr);

    void setModel(const SessionBaseModel *model);
    void setUser(std::shared_ptr<User> user);

    void initKeyboardLayoutList();

signals:
    void requestSwitchUser();
    void requestShutdown();
    void requestSwitchSession();
    void requestSwitchVirtualKB();
    void requestKeyboardLayout(const QPoint &pos);
    void requestShowModule(const QString &name);
    void notifyKeyboardLayoutHidden();

public slots:
    void addModule(dss::module::BaseModuleInterface *module);
    void removeModule(dss::module::BaseModuleInterface *module);
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void initConnect();
    void showTips();
    void hideTips();
    void updateLayout();
    void updateTapOrder();

private:
    int m_index = 0;
    QList<DFloatingButton *> m_btnList;

    QHBoxLayout *m_mainLayout = nullptr;
    DFloatingButton *m_virtualKBBtn = nullptr;
    DFloatingButton *m_switchUserBtn = nullptr;
    DFloatingButton *m_powerBtn = nullptr;
    DFloatingButton *m_sessionBtn = nullptr;
    QLabel *m_sessionTip = nullptr;
    QWidget *m_tipWidget = nullptr;
#ifndef SHENWEI_PLATFORM
    QPropertyAnimation *m_tipsAni = nullptr;
#endif
    QMap<QString, QWidget *> m_modules;
    QMenu *m_contextMenu;
    TipsWidget *m_tipsWidget;
    const SessionBaseModel *m_model;

    DArrowRectangle *m_arrowRectWidget;
    KBLayoutListView *m_kbLayoutListView;   // 键盘布局列表
    DFloatingButton *m_keyboardBtn;         // 键盘布局按钮
    std::shared_ptr<User> m_curUser;
    QList<QMetaObject::Connection> m_connectionList;
    bool m_onboardBtnVisible;
    DTK_CORE_NAMESPACE::DConfig *m_dconfig;
};

#endif // CONTROLWIDGET_H
