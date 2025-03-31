// SPDX-FileCopyrightText: 2011 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controlwidget.h"
#include "sessionbasemodel.h"
#include "kblayoutlistview.h"
#include "modules_loader.h"
#include "tipswidget.h"
#include "public_func.h"
#include "plugin_manager.h"
#include "dconfig_helper.h"
#include "roundpopupwidget.h"
#include "tipcontentwidget.h"
#include "sessionpopupwidget.h"
#include "userlistpopupwidget.h"
#include "sessionmanager.h"

#include <DFloatingButton>
#include <DArrowRectangle>
#include <DPushButton>
#include <DRegionMonitor>

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QMenu>
#include <QWindow>

#define BUTTON_ICON_SIZE QSize(26,26)
#define BUTTON_SIZE QSize(52,52)
static constexpr int TipsBottomDistance = 10;
static constexpr int HorizontalMargin = 10;
static constexpr int VerticalMargin = 10;

const QString NetworkPlugin = "network-item-key";

using namespace dss;
DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE

bool FloatingButton::eventFilter(QObject *watch, QEvent *event)
{
    if (watch == this) {
        if (event->type() == QEvent::Hide) {
            Q_EMIT buttonHide();
            return false;
        }
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonRelease) {
            m_State = underMouse() ? Hover : Normal;
            if (e->button() == Qt::RightButton) {
                Q_EMIT requestShowMenu();
            }
        } else if (event->type() == QEvent::Enter) {
            m_State = Hover;
            Q_EMIT requestShowTips();
        } else if (event->type() == QEvent::Leave) {
            m_State = Normal;
            Q_EMIT requestHideTips();
        } else if (event->type() == QEvent::MouseButtonPress) {
            if (e->button() == Qt::LeftButton)
                m_State = Press;
            Q_EMIT requestHideTips();
        }
    }

    return false;
}

void FloatingButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    DStylePainter p(this);
    DStyleOptionButton opt;
    initStyleOption(&opt);
    switch (m_State)
    {
    case Hover:
        p.setOpacity(1.0);
        break;
    case Press:
        p.setOpacity(0.4);
        break;
    case Normal:
        p.setOpacity(0.7);
        break;
    default:
        break;
    }
    p.drawControl(DStyle::CE_IconButton, opt);
}

ControlWidget::ControlWidget(const SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_contextMenu(new QMenu(this))
    , m_tipsWidget(new TipsWidget(parent ? parent->window() : nullptr))
    , m_roundPopupWidget(new RoundPopupWidget(this))
{
    m_tipsWidget->setVisible(false);
    m_roundPopupWidget->setVisible(false);

    setModel(model);
    initUI();
    initConnect();
}

ControlWidget::~ControlWidget()
{
    if (m_tipsWidget) {
        m_tipsWidget->deleteLater();
    }
}

void ControlWidget::setModel(const SessionBaseModel *model)
{
    m_model = model;
    setUser(model->currentUser());
    connect(m_model, &SessionBaseModel::onStatusChanged, this, &ControlWidget::updateModuleVisible);
}

void ControlWidget::setUser(std::shared_ptr<User> user)
{
    for (const QMetaObject::Connection &connection : std::as_const(m_connectionList))
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
    m_kbLayoutListView->installEventFilter(this);
    m_kbLayoutListView->setAccessibleName(QStringLiteral("KbLayoutlistview"));
    m_kbLayoutListView->initData(m_curUser->keyboardLayoutList());
    m_kbLayoutListView->setMinimumWidth(DDESESSIONCC::KEYBOARD_LAYOUT_WIDTH);
    m_kbLayoutListView->setMaximumSize(DDESESSIONCC::KEYBOARD_LAYOUT_WIDTH, DDESESSIONCC::LAYOUT_BUTTON_HEIGHT * 7);
    m_kbLayoutListView->setFocusPolicy(Qt::NoFocus);
    m_kbLayoutListView->setVisible(false);

    const QStringList languageList = language.split(";");
    if (!languageList.isEmpty())
        static_cast<QAbstractButton *>(m_keyboardBtn)->setText(languageList.at(0).toUpper());

    connect(m_kbLayoutListView, &KBLayoutListView::itemClicked, this, &ControlWidget::onItemClicked);
}

void ControlWidget::updatePluginVisible(const QString module, bool state)
{
    qCInfo(DDE_SHELL) << "Update plugin visible, state: " << state;
    for (auto key : m_modules.keys()) {
        if (key && key->objectName() == module) {
            m_modules.value(key)->setVisible(state);
            m_modulesVisible.insert(module, state);

            if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
                m_modules.value(key)->setVisible(false);

            return;
        }
    }
}

void ControlWidget::setVirtualKBVisible(bool visible)
{
    m_onboardBtnVisible = visible;
    const bool hideOnboard = DConfigHelper::instance()->getConfig("hideOnboard", false).toBool();
    m_virtualKBBtn->setVisible(visible && !hideOnboard);
}

void ControlWidget::initUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 60, 0);
    m_mainLayout->setSpacing(26);
    m_mainLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    m_sessionBtn = new FloatingButton(this);
    m_sessionBtn->setIconSize(BUTTON_ICON_SIZE);
    m_sessionBtn->setFixedSize(BUTTON_SIZE);
    m_sessionBtn->setAutoExclusive(true);
    m_sessionBtn->setBackgroundRole(DPalette::Button);
    m_sessionBtn->hide();
    m_sessionBtn->setTipText(tr("Desktop Environment and Display Protocol"));

    m_keyboardBtn = new FloatingButton(this);
    m_keyboardBtn->setAccessibleName("KeyboardLayoutBtn");
    m_keyboardBtn->setFixedSize(BUTTON_SIZE);
    m_keyboardBtn->setAutoExclusive(true);
    m_keyboardBtn->setBackgroundRole(DPalette::Button);
    m_keyboardBtn->setAutoExclusive(true);
    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(QString());
    m_keyboardBtn->installEventFilter(this);
    m_keyboardBtn->setTipText(tr("Keyboard Layout"));

    // 给显示文字的按钮设置样式
    QPalette pal = m_keyboardBtn->palette();
    pal.setColor(QPalette::Window, QColor(Qt::red));
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    pal.setColor(QPalette::Highlight, Qt::white);
    m_keyboardBtn->setPalette(pal);

    m_virtualKBBtn = new FloatingButton(this);
    m_virtualKBBtn->setAccessibleName("VirtualKeyboardBtn");
    m_virtualKBBtn->setIcon(QIcon::fromTheme(":/img/screen_keyboard_hover.svg"));
    m_virtualKBBtn->hide();
    m_virtualKBBtn->setIconSize(BUTTON_ICON_SIZE);
    m_virtualKBBtn->setFixedSize(BUTTON_SIZE);
    m_virtualKBBtn->setAutoExclusive(true);
    m_virtualKBBtn->setBackgroundRole(DPalette::Button);
    m_virtualKBBtn->installEventFilter(this);
    m_virtualKBBtn->setTipText(tr("Onboard"));

    m_switchUserBtn = new FloatingButton(this);
    m_switchUserBtn->setAccessibleName("SwitchUserBtn");
    m_switchUserBtn->setIcon(QIcon::fromTheme(":/img/bottom_actions/userswitch_hover.svg"));
    m_switchUserBtn->setIconSize(BUTTON_ICON_SIZE);
    m_switchUserBtn->setFixedSize(BUTTON_SIZE);
    m_switchUserBtn->setAutoExclusive(true);
    m_switchUserBtn->setBackgroundRole(DPalette::Button);
    m_switchUserBtn->installEventFilter(this);
    m_switchUserBtn->setTipText(tr("Switch User"));

    m_powerBtn = new FloatingButton(this);
    m_powerBtn->setAccessibleName("PowerBtn");
    m_powerBtn->setIcon(QIcon(":/img/bottom_actions/shutdown_hover.svg"));
    m_powerBtn->setIconSize(BUTTON_ICON_SIZE);
    m_powerBtn->setFixedSize(BUTTON_SIZE);
    m_powerBtn->setAutoExclusive(true);
    m_powerBtn->setBackgroundRole(DPalette::Button);
    m_powerBtn->installEventFilter(this);
    m_powerBtn->setTipText(tr("Power"));

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

    updateLayout();

    // 初始化键盘布局列表
    initKeyboardLayoutList();

    DConfigHelper::instance()->bind(this, "hideOnboard", &ControlWidget::onDConfigPropertyChanged);
}

void ControlWidget::initConnect()
{
    connect(m_sessionBtn, &FloatingButton::clicked, this, &ControlWidget::showSessionPopup);
    connect(m_sessionBtn, &FloatingButton::requestShowTips, this, &ControlWidget::showInfoTips);
    connect(m_sessionBtn, &FloatingButton::requestHideTips, this, &ControlWidget::hideInfoTips);

    connect(m_switchUserBtn, &FloatingButton::clicked, this, &ControlWidget::showUserListPopupWidget);
    connect(m_switchUserBtn, &FloatingButton::requestShowTips, this, &ControlWidget::showInfoTips);
    connect(m_switchUserBtn, &FloatingButton::requestHideTips, this, &ControlWidget::hideInfoTips);

    connect(m_powerBtn, &FloatingButton::clicked, this, &ControlWidget::requestShutdown);
    connect(m_powerBtn, &FloatingButton::requestShowTips, this, &ControlWidget::showInfoTips);
    connect(m_powerBtn, &FloatingButton::requestHideTips, this, &ControlWidget::hideInfoTips);

    connect(m_keyboardBtn, &FloatingButton::clicked, this, &ControlWidget::setKBLayoutVisible);
    connect(m_keyboardBtn, &FloatingButton::requestShowTips, this, &ControlWidget::showInfoTips);
    connect(m_keyboardBtn, &FloatingButton::requestHideTips, this, &ControlWidget::hideInfoTips);

    connect(m_virtualKBBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchVirtualKB);
    connect(m_virtualKBBtn, &FloatingButton::requestShowTips, this, &ControlWidget::showInfoTips);
    connect(m_virtualKBBtn, &FloatingButton::requestHideTips, this, &ControlWidget::hideInfoTips);

    connect(m_model, &SessionBaseModel::currentUserChanged, this, &ControlWidget::setUser);
    connect(m_model, &SessionBaseModel::hidePluginMenu, m_contextMenu, [this] {
        m_contextMenu->close();
        // 重置密码弹窗弹出的时候不重新抓取键盘，否则会导致重置密码弹窗无法抓取键盘
        m_doGrabKeyboard = false;
    });
}

QString ControlWidget::messageCallback(const QString &message, void *app_data)
{
    if (!app_data && static_cast<ControlWidget *>(app_data)) {
        qCWarning(DDE_SHELL) << "Appdata is null, app data: " << app_data;
        return QString();
    }
    QJsonObject retObj;
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(message.toLatin1(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        retObj["Code"] = "-1";
        retObj["Message"] = "Failed to analysis message info from plugin!";
        qCWarning(DDE_SHELL) << "Failed to analysis message info from plugin!: " << message;
        return toJson(retObj);
    }
    QJsonObject messageObj = messageDoc.object();

    QString cmd = messageObj.value("Cmd").toString();
    const QString pluginKey = messageObj.value("PluginKey").toString();
    if (cmd == "ShowContent") {
        emit static_cast<ControlWidget *>(app_data)->requestShowModule(pluginKey, true);
    } else if (cmd == "PluginVisible") {
        static_cast<ControlWidget *>(app_data)->updatePluginVisible(pluginKey, messageObj.value("Visible").toBool());
    }

    return message;
}

void ControlWidget::toggleButtonPopup(const FloatingButton *button, QWidget *popup)
{
    if (!m_roundPopupWidget || !button || !topLevelWidget())
        return;
    m_roundPopupWidget->setContent(popup);
    // 设计弹窗离右边距和底部按钮距离
    QPoint globalPos = mapToGlobal(button->pos()) - QPoint((m_roundPopupWidget->width() - button->width()) / 2,
                                                              m_roundPopupWidget->height() + VerticalMargin);
    // 保证页面显示没超出显示屏幕范围
    QRect topWidgetRect = topLevelWidget()->frameGeometry();
    if ((globalPos.x() + m_roundPopupWidget->width() + VerticalMargin) > (topWidgetRect.x() + topWidgetRect.width())) {
        int leftMoveValue = globalPos.x() + m_roundPopupWidget->width() + VerticalMargin - (topWidgetRect.x() + topWidgetRect.width());
        globalPos.setX(globalPos.x() - leftMoveValue);
    }
    Q_EMIT requestTogglePopup(globalPos, m_roundPopupWidget);
}

QWidget *ControlWidget::getTray(const QString &name)
{
    for (auto key : m_modules.keys()) {
        if (key && key->objectName() == name) {
            return m_modules.value(key);
        }
    }

    return nullptr;
}

void ControlWidget::addModule(TrayPlugin *trayModule)
{
    trayModule->init();

    FloatingButton *button = new FloatingButton(this);
    button->setIconSize(QSize(26, 26));
    button->setFixedSize(QSize(52, 52));
    button->setAutoExclusive(true);
    button->setBackgroundRole(DPalette::Button);

    QWidget *trayWidget = trayModule->itemWidget();
    if (trayWidget) {
        trayWidget->setParent(button);
        QHBoxLayout *layout = new QHBoxLayout(button);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(trayWidget);
        button->setVisible(trayWidget->isVisible());
        trayWidget->setObjectName(trayModule->key());
    } else {
        button->setIcon(QIcon(trayModule->icon()));
    }

    m_modules.insert(trayWidget, button);

    trayModule->setAppData(this);
    trayModule->setMessageCallback(&ControlWidget::messageCallback);

    // button的顺序与layout插入顺序保持一直
    m_btnList.insert(1, button);

    connect(button, &FloatingButton::requestShowMenu, this, [this, trayModule] {
        if (!m_canShowMenu)
            return;
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
        m_doGrabKeyboard = true;
        QAction *action = m_contextMenu->exec(QCursor::pos());
        if (action)
            trayModule->invokedMenuItem(action->data().toString(), true);

        // 右键菜单隐藏后需要重新grab键盘，否则会导致快捷键重新生效
        if (!m_model->isUseWayland() && m_doGrabKeyboard) {
            QWindow *winHandle = topLevelWidget()->windowHandle();
            if (!winHandle || !winHandle->setKeyboardGrabEnabled(true)) {
                qCWarning(DDE_SHELL) << "Grab keyboard failed";
            }
        }
    });

    connect(button, &FloatingButton::requestShowTips, this, [=] {
        if (trayModule->itemTipsWidget() && !trayModule->content()->isVisible()) {
            m_tipsWidget->setContent(trayModule->itemTipsWidget());
            // 因为密码框需要一直获取焦点，会导致TipsWidget在间隙时间内的Visible变为false
            // DArrowRectangle::show中当Visible为false时会activateWindow，抢占密码框焦点
            // 所以这里手动设置visible为true
            int x = mapToGlobal(button->pos()).x() + button->width() / 2;
            int y = mapToGlobal(button->pos()).y();
            m_tipsWidget->move(x, y);
            m_tipsWidget->setVisible(true);
            m_tipsWidget->show(x, y);
        }
    });

    connect(button, &FloatingButton::requestHideTips, this, [=] {
        if (m_tipsWidget->getContent())
            m_tipsWidget->getContent()->setVisible(false);
        m_tipsWidget->hide();
        update();
    });

    connect(button, &FloatingButton::clicked, this, [this, trayModule] {
        emit requestShowModule(trayModule->key());
    });

    updateLayout();
}

void ControlWidget::removeModule(TrayPlugin *trayModule)
{
    auto i = m_modules.constBegin();
    while (i != m_modules.constEnd()) {
        if (i.key() && i.key()->objectName() == trayModule->key()) {
            m_modules.remove(i.key());
            m_btnList.removeAll(dynamic_cast<DFloatingButton *>(i.value().data()));
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

void ControlWidget::setUserSwitchEnable(const bool visible)
{
    m_switchUserBtn->setVisible(visible);
    if (m_btnList.indexOf(m_switchUserBtn) == m_index) {
        m_index = 0;
    }

    updateTapOrder();
}

void ControlWidget::setSessionSwitchEnable(const bool visible)
{
    if (!visible)  {
        m_sessionBtn->hide();
        return;
    }

    m_sessionBtn->show();

#ifdef SHENWEI_PLATFORM
    m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
    m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
    m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");
#endif

    updateTapOrder();
}

void ControlWidget::chooseToSession(const QString &session)
{
    if (!m_sessionBtn || session.isEmpty()) {
        return;
    }

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
    FloatingButton *clickedButton = static_cast<FloatingButton *>(sender());
    if (!clickedButton)
        return;

    toggleButtonPopup(clickedButton, m_kbLayoutListView);
}

void ControlWidget::setKeyboardType(const QString &str)
{
    // 初始化当前键盘布局输入法类型
    QString currentText = str.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/"))
        currentText = currentText.split("/").last();

    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(currentText.toUpper());

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

    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(currentText.toUpper());
    Q_EMIT requestHidePopup();
    m_curUser->setKeyboardLayout(str);
}

void ControlWidget::updateModuleVisible()
{
    for (auto key : m_modules.keys()) {
        if (key) {
            const QString &module = key->objectName();
            bool visible = m_model->currentModeState() == SessionBaseModel::ModeStatus::PasswordMode;
            if(m_modulesVisible.contains(module)){
                visible &= m_modulesVisible.value(module);
            }
            m_modules.value(key)->setVisible(visible);
        }
    }
}

bool ControlWidget::eventFilter(QObject *watched, QEvent *event)
{
#ifndef SHENWEI_PLATFORM
    auto *btn = qobject_cast<DFloatingButton *>(watched);
    if (btn && m_btnList.contains(btn)) {
        if (event->type() == QEvent::Enter) {
            m_index = m_btnList.indexOf(btn);
        }
    }

    if (watched == m_kbLayoutListView && event->type() == QEvent::Hide) {
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
        m_index = it != m_btnList.end() ? m_btnList.indexOf(*it) : 0;
        break;
    }
}

void ControlWidget::showEvent(QShowEvent *event)
{
    // 对tray类型插件延时处理,避免阻塞主界面显示。
    QTimer::singleShot(500, this, [this]{
        auto initPlugins = [this] {
            for (const auto &module : PluginManager::instance()->trayPlugins()) {
                if (!module)
                    continue;

                addModule(module);
            }
        };
        static bool isInit = false;
        if (!isInit) {
            initPlugins();
            isInit = true;
            connect(PluginManager::instance(), &PluginManager::trayPluginAdded, this, [this](TrayPlugin *module) {
                if (!module)
                    return;

                addModule(module);
            });
        }

        for (auto key : m_modules.keys()) {
            auto value = m_modules.value(key);
            if (!value) {
                continue;
            }
            value->setVisible(key->isVisibleTo(this));
        }
    });

    QWidget::showEvent(event);
}

void ControlWidget::paintEvent(QPaintEvent *event)
{
    // QPainter painter(this);
    // painter.setRenderHint(QPainter::Antialiasing);
    // painter.setPen(Qt::NoPen);
    // painter.setBrush(Qt::red);
    //
    // painter.drawRect(rect());
    QWidget::paintEvent(event);
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

void ControlWidget::onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<ControlWidget*>(objPtr);
    if (!obj)
        return;

    if (obj->m_virtualKBBtn && key == "hideOnboard") {
        obj->m_virtualKBBtn->setVisible(obj->m_onboardBtnVisible && !value.toBool());
    }
}

void ControlWidget::showInfoTips()
{
    FloatingButton * button = qobject_cast<FloatingButton *>(sender());
    if (!button) {
        return;
    }

    if (!m_tipContentWidget) {
        m_tipContentWidget = new TipContentWidget();
    }

    m_tipContentWidget->setText(button->tipText());
    m_tipsWidget->setContent(m_tipContentWidget);

    qInfo() << "==============";
    QPoint p = m_tipsWidget->parentWidget() ? m_tipsWidget->parentWidget()->mapFromGlobal(mapToGlobal(button->pos())) : mapToGlobal(button->pos());
    m_tipsWidget->show(p.x() + button->width() / 2, p.y() - TipsBottomDistance);
}

void ControlWidget::hideInfoTips()
{
    if (m_tipsWidget->getContent()) {
        m_tipsWidget->getContent()->setVisible(false);
        m_tipsWidget->setContent(nullptr);
    }

    m_tipsWidget->hide();
}

void ControlWidget::showSessionPopup()
{
    if (!m_sessionPopupWidget) {
        m_sessionPopupWidget = new SessionPopupWidget(this);
        connect(m_sessionPopupWidget, &SessionPopupWidget::sessionItemClicked, [this](QString session) {
            Q_EMIT requestHidePopup();
            Q_EMIT requestSwitchSession(session);
        });

        m_sessionPopupWidget->setSessionInfo(SessionManager::Reference().sessionInfo(),
                                             SessionManager::Reference().currentSession());
    } else {
        m_sessionPopupWidget->updateCurrentSession(SessionManager::Reference().currentSession());
    }
    toggleButtonPopup(m_sessionBtn, m_sessionPopupWidget);
}

void ControlWidget::showUserListPopupWidget()
{
    if (!m_userListPopupWidget) {
        m_userListPopupWidget = new UserListPopupWidget(m_model, this);
        connect(m_userListPopupWidget, &UserListPopupWidget::requestSwitchToUser, [ this ](std::shared_ptr<User> user) {
            Q_EMIT requestHidePopup();
            Q_EMIT requestSwitchUser(user);
        });
    }
    toggleButtonPopup(m_switchUserBtn, m_userListPopupWidget);
}
