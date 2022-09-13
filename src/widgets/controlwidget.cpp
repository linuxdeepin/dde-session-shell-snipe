// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controlwidget.h"
#include "sessionbasemodel.h"
#include "kblayoutlistview.h"

#include "mediawidget.h"
#include "modules_loader.h"
#include "tray_module_interface.h"
#include "tipswidget.h"
#include "public_func.h"

#include <DFloatingButton>
#include <DArrowRectangle>
#include <DPushButton>
#include <dimagebutton.h>
#include <DConfig>
#include <DRegionMonitor>

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QWheelEvent>
#include <QMenu>
#include <QWindow>

#define BUTTON_ICON_SIZE QSize(26,26)
#define BUTTON_SIZE QSize(52,52)

using namespace dss;
DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE

bool FlotingButton::eventFilter(QObject *watch, QEvent *event)
{
    if (watch == this) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::RightButton) {
                Q_EMIT requestShowMenu();
            }
        } else if (event->type() == QEvent::Enter) {
            Q_EMIT requestShowTips();
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress) {
            Q_EMIT requestHideTips();
        }
    }

    return false;
}

ControlWidget::ControlWidget(const SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_contextMenu(new QMenu(this))
    , m_tipsWidget(new TipsWidget(this))
    , m_arrowRectWidget(new DArrowRectangle(DArrowRectangle::ArrowBottom, this))
    , m_kbLayoutListView(nullptr)
    , m_keyboardBtn(nullptr)
    , m_onboardBtnVisible(true)
    , m_dconfig(DConfig::create(getDefaultConfigFileName(), getDefaultConfigFileName(), QString(), this))
{
    setModel(model);
    initUI();
    initConnect();
}

void ControlWidget::setModel(const SessionBaseModel *model)
{
    m_model = model;
    setUser(model->currentUser());
    connect(m_model, &SessionBaseModel::onStatusChanged, this, &ControlWidget::updateModuleVisible);
}

void ControlWidget::setUser(std::shared_ptr<User> user)
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connectionList))
        user.get()->disconnect(connection);

    m_connectionList.clear();

    m_curUser = user;

    m_connectionList << connect(m_curUser.get(), &User::keyboardLayoutChanged, this, &ControlWidget::setKeyboardType)
                     << connect(m_curUser.get(), &User::keyboardLayoutListChanged, this, &ControlWidget::setKeyboardList);
}

void ControlWidget::initKeyboardLayoutList()
{
    /* 键盘布局选择菜单 */
    const QString language = m_curUser->keyboardLayout();
    m_kbLayoutListView = new KBLayoutListView(language, this);
    m_kbLayoutListView->setAccessibleName(QStringLiteral("KbLayoutlistview"));
    m_kbLayoutListView->initData(m_curUser->keyboardLayoutList());
    m_kbLayoutListView->setMinimumWidth(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH);
    m_kbLayoutListView->setMaximumSize(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH, DDESESSIONCC::LAYOUTBUTTON_HEIGHT * 7);
    m_kbLayoutListView->setFocusPolicy(Qt::NoFocus);
    m_kbLayoutListView->setVisible(false);

    const QStringList languageList = language.split(";");
    if (!languageList.isEmpty())
        static_cast<QAbstractButton *>(m_keyboardBtn)->setText(languageList.at(0));

    // 无特效模式时，让窗口圆角
    m_arrowRectWidget->setProperty("_d_radius_force", true);
    m_arrowRectWidget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    m_arrowRectWidget->setMargin(0);
    m_arrowRectWidget->setShadowBlurRadius(20);
    m_arrowRectWidget->setRadius(6);
    m_arrowRectWidget->setShadowYOffset(2);
    m_arrowRectWidget->setShadowXOffset(0);
    m_arrowRectWidget->setArrowWidth(18);
    m_arrowRectWidget->setArrowHeight(10);
    m_arrowRectWidget->setMinimumWidth(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH);
    m_arrowRectWidget->setMaximumSize(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH, DDESESSIONCC::LAYOUTBUTTON_HEIGHT * 7);
    m_arrowRectWidget->setFocusPolicy(Qt::NoFocus);
    m_arrowRectWidget->setBackgroundColor(QColor(235, 235, 235, int(0.05 * 255)));
    m_arrowRectWidget->installEventFilter(this);

    QPalette pal = m_arrowRectWidget->palette();
    pal.setColor(DPalette::Inactive, DPalette::Base, QColor(235, 235, 235, int(0.05 * 255)));
    setPalette(pal);

    connect(m_kbLayoutListView, &KBLayoutListView::itemClicked, this, &ControlWidget::onItemClicked);
}

void ControlWidget::setVirtualKBVisible(bool visible)
{
    m_onboardBtnVisible = visible;
    m_virtualKBBtn->setVisible(visible && !m_dconfig->value("hideOnboard", false).toBool());
}

void ControlWidget::initUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 60, 0);
    m_mainLayout->setSpacing(26);
    m_mainLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    m_sessionBtn = new DFloatingButton(this);
    m_sessionBtn->setIconSize(BUTTON_ICON_SIZE);
    m_sessionBtn->setFixedSize(BUTTON_SIZE);
    m_sessionBtn->setAutoExclusive(true);
    m_sessionBtn->setBackgroundRole(DPalette::Button);
    m_sessionBtn->hide();

    m_keyboardBtn = new DFloatingButton(this);
    m_keyboardBtn->setAccessibleName("KeyboardLayoutBtn");
    m_keyboardBtn->setFixedSize(BUTTON_SIZE);
    m_keyboardBtn->setAutoExclusive(true);
    m_keyboardBtn->setBackgroundRole(DPalette::Button);
    m_keyboardBtn->setAutoExclusive(true);
    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(QString());
    m_keyboardBtn->installEventFilter(this);

    // 给显示文字的按钮设置样式
    QPalette pal = m_keyboardBtn->palette();
    pal.setColor(QPalette::Window, QColor(Qt::red));
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    m_keyboardBtn->setPalette(pal);

    m_virtualKBBtn = new DFloatingButton(this);
    m_virtualKBBtn->setAccessibleName("VirtualKeyboardBtn");
    m_virtualKBBtn->setIcon(QIcon::fromTheme(":/img/screen_keyboard_normal.svg"));
    m_virtualKBBtn->hide();
    m_virtualKBBtn->setIconSize(BUTTON_ICON_SIZE);
    m_virtualKBBtn->setFixedSize(BUTTON_SIZE);
    m_virtualKBBtn->setAutoExclusive(true);
    m_virtualKBBtn->setBackgroundRole(DPalette::Button);
    m_virtualKBBtn->installEventFilter(this);

    m_switchUserBtn = new DFloatingButton(this);
    m_switchUserBtn->setAccessibleName("SwitchUserBtn");
    m_switchUserBtn->setIcon(QIcon::fromTheme(":/img/bottom_actions/userswitch_normal.svg"));
    m_switchUserBtn->setIconSize(BUTTON_ICON_SIZE);
    m_switchUserBtn->setFixedSize(BUTTON_SIZE);
    m_switchUserBtn->setAutoExclusive(true);
    m_switchUserBtn->setBackgroundRole(DPalette::Button);
    m_switchUserBtn->installEventFilter(this);

    m_powerBtn = new DFloatingButton(this);
    m_powerBtn->setAccessibleName("PowerBtn");
    m_powerBtn->setIcon(QIcon(":/img/bottom_actions/shutdown_normal.svg"));
    m_powerBtn->setIconSize(BUTTON_ICON_SIZE);
    m_powerBtn->setFixedSize(BUTTON_SIZE);
    m_powerBtn->setAutoExclusive(true);
    m_powerBtn->setBackgroundRole(DPalette::Button);
    m_powerBtn->installEventFilter(this);

    m_btnList.append(m_sessionBtn);
    m_btnList.append(m_keyboardBtn);
    m_btnList.append(m_virtualKBBtn);
    m_btnList.append(m_switchUserBtn);
    m_btnList.append(m_powerBtn);

    if (m_curUser->keyboardLayoutList().size() < 2)
        m_keyboardBtn->hide();

    m_mainLayout->addWidget(m_sessionBtn);
    m_mainLayout->addWidget(m_keyboardBtn);
    m_mainLayout->addWidget(m_virtualKBBtn);
    m_mainLayout->addWidget(m_switchUserBtn);
    m_mainLayout->addWidget(m_powerBtn);

    for (QSharedPointer<module::BaseModuleInterface> module : module::ModulesLoader::instance().moduleList()) {
        if (module.isNull())
            continue;

        addModule(module.data());
    }

    updateLayout();

    // 初始化键盘布局列表
    initKeyboardLayoutList();
}

void ControlWidget::initConnect()
{
    connect(&module::ModulesLoader::instance(), &module::ModulesLoader::moduleFound, this, &ControlWidget::addModule);
    connect(m_sessionBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchSession);
    connect(m_switchUserBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchUser);
    connect(m_powerBtn, &DFloatingButton::clicked, this, &ControlWidget::requestShutdown);
    connect(m_virtualKBBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchVirtualKB);
    connect(m_keyboardBtn, &DFloatingButton::clicked, this, &ControlWidget::setKBLayoutVisible);
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &ControlWidget::setUser);
    connect(m_dconfig, &DConfig::valueChanged, this, [this] (const QString &key) {
        if (m_virtualKBBtn && key == "hideOnboard") {
            m_virtualKBBtn->setVisible(m_onboardBtnVisible && !m_dconfig->value("hideOnboard", false).toBool());
        }
    });
    connect(m_model, &SessionBaseModel::hidePluginMenu, m_contextMenu, &QMenu::close);
}

void ControlWidget::addModule(module::BaseModuleInterface *module)
{
    if (module && module->type() != module::BaseModuleInterface::TrayType)
        return;

    module::TrayModuleInterface *trayModule = dynamic_cast<module::TrayModuleInterface *>(module);
    if (!trayModule)
        return;

    trayModule->init();

    FlotingButton *button = new FlotingButton(this);
    button->setIconSize(QSize(26, 26));
    button->setFixedSize(QSize(52, 52));
    button->setAutoExclusive(true);
    button->setBackgroundRole(DPalette::Button);

    if (QWidget *trayWidget = trayModule->itemWidget()) {
        trayWidget->setParent(this);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->addWidget(trayWidget);

        button->setLayout(layout);
    } else {
        button->setIcon(QIcon(trayModule->icon()));
    }

    m_modules.insert(trayModule->key(), button);
    // button的顺序与layout插入顺序保持一直
    m_btnList.insert(1, button);

    connect(button, &FlotingButton::requestShowMenu, this, [ = ] {
        const QString menuJson = trayModule->itemContextMenu();
        if (menuJson.isEmpty())
            return;

        QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
        if (jsonDocument.isNull())
            return;

        QJsonObject jsonMenu = jsonDocument.object();

        m_contextMenu->clear();
        QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
        for (auto item : jsonMenuItems) {
            QJsonObject itemObj = item.toObject();
            QAction *action = new QAction(itemObj.value("itemText").toString());
            action->setCheckable(itemObj.value("isCheckable").toBool());
            action->setChecked(itemObj.value("checked").toBool());
            action->setData(itemObj.value("itemId").toString());
            action->setEnabled(itemObj.value("isActive").toBool());
            m_contextMenu->addAction(action);
        }

        QAction *action = m_contextMenu->exec(QCursor::pos());
        if (action)
            trayModule->invokedMenuItem(action->data().toString(), true);

        // 右键菜单隐藏后需要重新grab键盘，否则会导致快捷键重新生效
        if (!m_model->isUseWayland()) {
            QWindow *winHandle = topLevelWidget()->windowHandle();
            if (!winHandle || !winHandle->setKeyboardGrabEnabled(true)) {
                qWarning() << "Grab keyboard failed";
            }
        }
    });

    connect(button, &FlotingButton::requestShowTips, this, [ = ] {
        if (trayModule->itemTipsWidget()) {
            m_tipsWidget->setContent(trayModule->itemTipsWidget());
            m_tipsWidget->show(mapToGlobal(button->pos()).x() + button->width() / 2,mapToGlobal(button->pos()).y());
        }
    });

    connect(button, &FlotingButton::requestHideTips, this, [ = ] {
        if (m_tipsWidget->getContent())
            m_tipsWidget->getContent()->setVisible(false);
        m_tipsWidget->hide();
    });

    connect(button, &DFloatingButton::clicked, this, [this, trayModule] {
        emit requestShowModule(trayModule->key());
    }, Qt::UniqueConnection);


    updateLayout();
}

void ControlWidget::removeModule(module::BaseModuleInterface *module)
{
    if (module->type() != module::BaseModuleInterface::TrayType)
        return;

    module::TrayModuleInterface *trayModule = dynamic_cast<module::TrayModuleInterface *>(module);

    QMap<QString, QWidget *>::const_iterator i = m_modules.constBegin();
    while (i != m_modules.constEnd()) {
        if (i.key() == trayModule->key()) {
            m_modules.remove(i.key());
            m_btnList.removeAll(dynamic_cast<DFloatingButton *>(i.value()));
            updateLayout();
            break;
        }
        ++i;
    }
}

void ControlWidget::updateLayout()
{
    QObjectList moduleWidgetList = m_mainLayout->children();
    for (QWidget *moduleWidget : m_modules.values()) {
        if (moduleWidgetList.contains(moduleWidget)) {
            moduleWidgetList.removeAll(moduleWidget);
        }
        m_mainLayout->insertWidget(1, moduleWidget);
    }
    for (QObject *moduleWidget : moduleWidgetList) {
        m_mainLayout->removeWidget(qobject_cast<QWidget *>(moduleWidget));
    }

    updateTapOrder();
}

void ControlWidget::showTips()
{
#ifndef SHENWEI_PLATFORM
    m_tipsAni->setStartValue(QPoint(m_tipWidget->width(), 0));
    m_tipsAni->setEndValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(0, 0);
#endif
}

void ControlWidget::hideTips()
{
#ifndef SHENWEI_PLATFORM
    //在退出动画时候会出现白边，+1
    m_tipsAni->setEndValue(QPoint(m_tipWidget->width() + 1, 0));
    m_tipsAni->setStartValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(m_tipWidget->width() + 1, 0);
#endif
}

void ControlWidget::setUserSwitchEnable(const bool visible)
{
    m_switchUserBtn->setVisible(visible);
    if (m_btnList.indexOf(m_switchUserBtn) == m_index) {
        m_index = 0;
    }
}

void ControlWidget::setSessionSwitchEnable(const bool visible)
{
    if (!visible)  {
        m_sessionBtn->hide();
        return;
    }

        m_sessionBtn->show();
#ifndef SHENWEI_PLATFORM
        m_sessionBtn->installEventFilter(this);
#else
        m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
        m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
        m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");

#endif

    if (!m_tipWidget) {
        m_tipWidget = new QWidget;
        m_tipWidget->setAccessibleName("TipWidget");
        m_mainLayout->insertWidget(0, m_tipWidget);
        m_mainLayout->setAlignment(m_tipWidget, Qt::AlignCenter);
    }

    if (!m_sessionTip) {
        m_sessionTip = new QLabel(m_tipWidget);
        m_sessionTip->setAccessibleName("SessionTip");
        m_sessionTip->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_sessionTip->installEventFilter(this);

#ifndef SHENWEI_PLATFORM
        m_sessionTip->setStyleSheet("color:white;"
                                    "font-size:12px;");
#else
        QPalette pe;
        pe.setColor(QPalette::WindowText, Qt::white);
        m_sessionTip->setPalette(pe);
#endif

        QGraphicsDropShadowEffect *tipsShadow = new QGraphicsDropShadowEffect(m_sessionTip);
        tipsShadow->setColor(Qt::white);
        tipsShadow->setBlurRadius(4);
        tipsShadow->setOffset(0, 0);
        m_sessionTip->setGraphicsEffect(tipsShadow);

        m_sessionTip->move(m_tipWidget->width(), 0);
    }

#ifndef SHENWEI_PLATFORM
    if (!m_tipsAni) {
        m_tipsAni = new QPropertyAnimation(m_sessionTip, "pos", this);
    }
#endif

    updateTapOrder();
}

void ControlWidget::chooseToSession(const QString &session)
{
    if (m_sessionBtn && m_sessionTip) {
        qDebug() << "chosen session: " << session;
        if (session.isEmpty())
            return;

        m_sessionTip->setText(session);
        if (session == "deepin") {
            m_sessionTip->setText("X11");
        }

        m_sessionTip->adjustSize();
        //当session长度改变时，应该移到它的width来隐藏
        m_sessionTip->move(m_sessionTip->size().width() + 1, 0);
        const QString sessionId = session.toLower();
        QString normalIcon = QString(":/img/sessions/%1_indicator_normal.svg").arg(sessionId);
        if (sessionId == "deepin") {
            normalIcon = QString(":/img/sessions/%1_indicator_normal.svg").arg("x11");
        }

        if (QFile(normalIcon).exists()) {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(normalIcon));
#else
            m_sessionBtn->setProperty("normalIcon", normalIcon);
            m_sessionBtn->setProperty("hoverIcon", hoverIcon);
            m_sessionBtn->setProperty("checkedIcon", checkedIcon);
#endif
        } else {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(":/img/sessions/unknown_indicator_normal.svg"));
#else
            m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
            m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
            m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");
#endif
        }
    }
}

void ControlWidget::leftKeySwitch()
{
    if (m_index == 0) {
        m_index = m_btnList.length() - 1;
    } else {
        --m_index;
    }

    for (int i = m_btnList.size(); i != 0; --i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

void ControlWidget::rightKeySwitch()
{
    if (m_index == m_btnList.size() - 1) {
        m_index = 0;
    } else {
        ++m_index;
    }

    for (int i = 0; i < m_btnList.size(); ++i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

void ControlWidget::setKBLayoutVisible()
{
    DFloatingButton *layoutButton = static_cast<DFloatingButton *>(sender());
    if (!layoutButton)
        return;

    if (!m_arrowRectWidget->getContent()) {
        m_arrowRectWidget->setContent(m_kbLayoutListView);
    }
    m_arrowRectWidget->resize(m_kbLayoutListView->size() + QSize(0, 10));

    QPoint pos = QPoint(mapToGlobal(layoutButton->pos()).x() + layoutButton->width() / 2, mapToGlobal(layoutButton->pos()).y());
    m_arrowRectWidget->move(pos.x(), pos.y());
    m_arrowRectWidget->setVisible(!m_arrowRectWidget->isVisible());
}

void ControlWidget::setKeyboardType(const QString &str)
{
    // 初始化当前键盘布局输入法类型
    QString currentText = str.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/"))
        currentText = currentText.split("/").last();

    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(currentText);

    // 更新键盘布局列表
    m_kbLayoutListView->updateList(str);
}

void ControlWidget::setKeyboardList(const QStringList &str)
{
    if (str.size() < 2)
        m_keyboardBtn->hide();
    else
        m_keyboardBtn->show();

    // 初始化当前键盘布局列表
    if (m_kbLayoutListView)
        m_kbLayoutListView->initData(str);
}

void ControlWidget::onItemClicked(const QString &str)
{
    // 初始化当前键盘布局输入法类型
    QString currentText = str.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/"))
        currentText = currentText.split("/").last();

    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(currentText);
    m_arrowRectWidget->hide();
    m_curUser->setKeyboardLayout(str);
}

void ControlWidget::updateModuleVisible()
{
    for (QWidget *moduleWidget : m_modules.values()) {
        moduleWidget->setVisible(m_model->currentModeState() == SessionBaseModel::ModeStatus::PasswordMode);
    }
}

bool ControlWidget::eventFilter(QObject *watched, QEvent *event)
{
#ifndef SHENWEI_PLATFORM
    if (watched == m_sessionBtn) {
        if (event->type() == QEvent::Enter)
            showTips();
        else if (event->type() == QEvent::Leave)
            hideTips();
    }

    if (watched == m_sessionTip) {
        if (event->type() == QEvent::Resize) {
            m_tipWidget->setFixedSize(m_sessionTip->size());
        }
    }

    DFloatingButton *btn = qobject_cast<DFloatingButton *>(watched);
    if (btn && m_btnList.contains(btn)) {
        if (event->type() == QEvent::Enter) {
            m_index = m_btnList.indexOf(btn);
        }
    }

    if (watched == m_arrowRectWidget && event->type() == QEvent::Hide) {
        emit notifyKeyboardLayoutHidden();
    }

#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return false;
}

void ControlWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        leftKeySwitch();
        break;
    case Qt::Key_Right:
        rightKeySwitch();
        break;
    case Qt::Key_Tab:
        auto it = std::find_if(m_btnList.begin(), m_btnList.end(),
                               [](DFloatingButton *btn) { return btn->hasFocus(); });
        m_index = it != m_btnList.end() ? m_btnList.indexOf(it.i->t()) : 0;
        break;
    }
}

void ControlWidget::updateTapOrder()
{
    // 找出所有按钮
    QList<DFloatingButton*> buttons;
    for(int i = 0; i < m_mainLayout->count(); ++i) {
        DFloatingButton *btn = qobject_cast<DFloatingButton *>(m_mainLayout->itemAt(i)->widget());
        if (btn)
            buttons.append(btn);
    }

    // 按从左到右的顺序设置tab order
    for (int i = 0; i < buttons.size(); ++i) {
        if ((i + 1) < buttons.size())
            setTabOrder(buttons[i], buttons[i + 1]);
    }
}
