// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "password_input_widget.h"
#include "keyboardmonitor.h"

#include <QResizeEvent>
#include <QTimer>
#include <QShowEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QPointer>
#include <QCoreApplication>
#include <DIconButton>
#include <DLabel>
#include <DHiDPIHelper>
#include <QDebug>
#include <QPainter>

const int kMerginSize = 5;
const int kIconSize = 24;
const int kBorderPadding = 10;
const int kContentMargin = 15;
const QString PASSWORD_HIDE = QStringLiteral(":/misc/images/password-hide.svg");
const QString PASSWORD_SHOWN = QStringLiteral(":/misc/images/password-shown.svg");
const QString CAPS_LOCK = QStringLiteral(":/misc/images/caps_lock.svg");

PasswordInputWidget::PasswordInputWidget(QWidget *parent)
    : DLineEdit(parent)
    , m_passwordShowBtn(nullptr)
    , m_capsLockLabel(nullptr)
    , m_longPressMode(true) // 默认启用长按模式
    , m_passwordVisible(false)
{
    setObjectName(QStringLiteral("PasswordInputWidget"));
    setAccessibleName(QStringLiteral("PasswordInputWidget"));

    // 设置密码输入框的基本属性
    setClearButtonEnabled(false);
    setEchoMode(QLineEdit::Password);
    setContextMenuPolicy(Qt::NoContextMenu);
    setFocusPolicy(Qt::StrongFocus);
    setCopyEnabled(false);
    setCutEnabled(false);
    setPasteEnabled(false);
    lineEdit()->setAcceptDrops(false);

    // 拦截 Cut/Copy/Paste/Undo/Redo 快捷键与鼠标中键粘贴
    this->lineEdit()->installEventFilter(this);
    this->installEventFilter(this);

    initCapsLockLabel();
    initPasswordButton();
    initKeyboardMonitor();
    initConnections();

    setContentsMargins(0, 0, 0, 0);

    // 设置文本居中,获取焦点时占位字符串隐藏
    lineEdit()->setAlignment(Qt::AlignCenter);
}

void PasswordInputWidget::initKeyboardMonitor()
{
    // 连接键盘监视器的capslock状态变化信号
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::capsLockStatusChanged,
            this, &PasswordInputWidget::onCapsLockStatusChanged);

    // 设置初始的capslock状态
    setCapsLockVisible(KeyboardMonitor::instance()->isCapsLockOn());
}

void PasswordInputWidget::onCapsLockStatusChanged(bool on)
{
    setCapsLockVisible(on);
}

void PasswordInputWidget::initCapsLockLabel()
{
    // 创建CapsLock状态标签
    m_capsLockLabel = new DLabel(this);
    m_capsLockLabel->setAccessibleName(QStringLiteral("CapsLockLabel"));

    // 加载CapsLock图标
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(CAPS_LOCK);
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_capsLockLabel->setPixmap(pixmap);
    m_capsLockLabel->setFixedSize(24, 24); // 固定标签尺寸
    m_capsLockLabel->setVisible(false); // 默认隐藏

    // 初始化标签布局
    updateCapsLockLayout();
}

void PasswordInputWidget::initPasswordButton()
{
    // 创建密码显示按钮
    m_passwordShowBtn = new DIconButton(this);
    m_passwordShowBtn->setAccessibleName(QStringLiteral("PasswordShowButton"));
    m_passwordShowBtn->setContentsMargins(0, 0, 0, 0);
    m_passwordShowBtn->setFocusPolicy(Qt::NoFocus);
    m_passwordShowBtn->setCursor(Qt::ArrowCursor);
    m_passwordShowBtn->setFlat(true);
    m_passwordShowBtn->setIcon(QIcon(PASSWORD_SHOWN));
    m_passwordShowBtn->setIconSize(QSize(16, 16));
    m_passwordShowBtn->setVisible(true);
    m_passwordShowBtn->setFixedSize(24, 24); // 固定按钮尺寸

    // 连接按钮信号
    updatePasswordButtonConnections();

    // 初始化按钮布局
    updatePasswordButtonLayout();

    // 初始化文本边距
    updatePasswordTextMargins();
}

void PasswordInputWidget::initConnections()
{
    // 连接文本变化信号
    connect(this, &DLineEdit::textChanged, this, &PasswordInputWidget::onTextChanged);
    // 每次用户输入后清空 undo/redo 栈，防止密码历史被还原
    connect(this->lineEdit(), &QLineEdit::textEdited, this, &PasswordInputWidget::clearUndoRedoStack);
}

void PasswordInputWidget::updatePasswordButtonConnections()
{
    // 先断开所有连接
    if (m_passwordShowBtn) {
        m_passwordShowBtn->disconnect();
    }

    if (m_longPressMode) {
        // 长按模式：按下显示，释放隐藏
        connect(m_passwordShowBtn, &DIconButton::pressed, this, &PasswordInputWidget::onPasswordShowPressed);
        connect(m_passwordShowBtn, &DIconButton::released, this, &PasswordInputWidget::onPasswordShowReleased);
    } else {
        // 点击模式：点击切换显示/隐藏状态
        connect(m_passwordShowBtn, &DIconButton::clicked, this, &PasswordInputWidget::onPasswordShowClicked);
    }
}

void PasswordInputWidget::updateCapsLockLayout()
{
    if (!m_capsLockLabel)
        return;

    const int capsLockHeight = m_capsLockLabel->height();

    // 在预留的左侧空间内居中显示 CapsLock 标签
    int x = kMerginSize * 2;
    int y = (height() - capsLockHeight) / 2;

    m_capsLockLabel->move(x, y);
}

void PasswordInputWidget::updatePasswordButtonLayout()
{
    if (!m_passwordShowBtn)
        return;

    const int buttonHeight = m_passwordShowBtn->height();

    // 在预留的右侧空间内居中显示密码按钮
    int x = width() - m_passwordShowBtn->width() - kMerginSize;
    int y = (height() - buttonHeight) / 2;

    m_passwordShowBtn->move(x, y);
}

void PasswordInputWidget::updatePasswordTextMargins()
{
    QMargins textMargins = lineEdit()->textMargins();

    const int rightWidth = (m_passwordShowBtn && m_passwordShowBtn->isVisible()
                            ? kIconSize : 0);

    const int leftWidth = (m_capsLockLabel && m_capsLockLabel->isVisible()
                           ? kIconSize : 0);

    textMargins.setRight(rightWidth);

    const int displayTextWidth = lineEdit()->fontMetrics().horizontalAdvance(lineEdit()->displayText());
    const int diff = lineEdit()->width() - kBorderPadding
                     - (displayTextWidth + kContentMargin + textMargins.right() * 2);
    textMargins.setLeft(qMax(leftWidth, rightWidth + (diff < 0 ? diff : 0)));

    if (textMargins == lineEdit()->textMargins()) {
        return;
    }

    lineEdit()->setTextMargins(textMargins);
}

void PasswordInputWidget::resizeEvent(QResizeEvent *event)
{
    updateCapsLockLayout();
    updatePasswordButtonLayout();
    updatePasswordTextMargins();

    DLineEdit::resizeEvent(event);
}

void PasswordInputWidget::showEvent(QShowEvent *event)
{
    updatePasswordButtonLayout();
    updatePasswordTextMargins();

    DLineEdit::showEvent(event);
}

void PasswordInputWidget::paintEvent(QPaintEvent *event)
{
    if (lineEdit()->hasFocus() && lineEdit()->alignment() == Qt::AlignCenter
        && !lineEdit()->placeholderText().isEmpty() && lineEdit()->text().isEmpty()) {
        QPainter pa(this);
        QPalette pal = palette();
        QColor col = pal.text().color();
        col.setAlpha(128);
        QPen oldpen = pa.pen();
        pa.setPen(col);
        QTextOption option;
        option.setAlignment(Qt::AlignCenter);

        // 考虑文本边距绘制占位符
        QRect textRect = rect();
        QMargins margins = lineEdit()->textMargins();
        textRect.adjust(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        pa.drawText(textRect, lineEdit()->placeholderText(), option);
    }

    QWidget::paintEvent(event);
}

void PasswordInputWidget::onTextChanged(const QString &text)
{
    Q_UNUSED(text)
    // 文本变化时需要重新计算居中位置
    updatePasswordTextMargins();
}

void PasswordInputWidget::setCapsLockVisible(bool visible)
{
    if (m_capsLockLabel) {
        m_capsLockLabel->setVisible(visible);
        // 使用固定边距策略，无需在可见性变化时更新边距
        updateCapsLockLayout();
    }
    updatePasswordTextMargins();
}

bool PasswordInputWidget::isCapsLockVisible() const
{
    return m_capsLockLabel ? m_capsLockLabel->isVisible() : false;
}

void PasswordInputWidget::setPasswordShowButtonVisible(bool visible)
{
    if (m_passwordShowBtn) {
        m_passwordShowBtn->setVisible(visible);
        // 按钮可见性变化时立即更新文本边距
        updatePasswordTextMargins();
    }
}

bool PasswordInputWidget::isPasswordShowButtonVisible() const
{
    return m_passwordShowBtn ? m_passwordShowBtn->isVisible() : false;
}

void PasswordInputWidget::setLongPressMode(bool enabled)
{
    if (m_longPressMode != enabled) {
        m_longPressMode = enabled;

        // 重置密码可见状态
        if (m_passwordVisible) {
            updatePasswordVisibility(false);
        }

        updatePasswordButtonConnections();
    }
}

bool PasswordInputWidget::isLongPressMode() const
{
    return m_longPressMode;
}

void PasswordInputWidget::selectAll()
{
    lineEdit()->selectAll();
}

void PasswordInputWidget::clear()
{
    updatePasswordVisibility(false);
    DLineEdit::clear();
    // Qt 文档明确 setText() 会清空 undo/redo 历史；clear() 文档未承诺此行为。
    // 阻断 lineEdit 信号避免 DLineEdit::clear() 之后再触发一次 textChanged。
    QSignalBlocker blocker(lineEdit());
    lineEdit()->setText(QString());
}

void PasswordInputWidget::onPasswordShowPressed()
{
    if (m_longPressMode) {
        updatePasswordVisibility(true);
    }
}

void PasswordInputWidget::onPasswordShowReleased()
{
    if (m_longPressMode) {
        updatePasswordVisibility(false);
    }
}

void PasswordInputWidget::onPasswordShowClicked()
{
    if (!m_longPressMode) {
        updatePasswordVisibility(!m_passwordVisible);
    }
}

void PasswordInputWidget::updatePasswordVisibility(bool visible)
{
    if (m_passwordVisible == visible)
        return;

    m_passwordVisible = visible;

    // 切换密码可见性
    setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);

    // 更新按钮图标
    if (m_passwordShowBtn) {
        m_passwordShowBtn->setIcon(QIcon(visible ? PASSWORD_HIDE : PASSWORD_SHOWN));
    }
}

bool PasswordInputWidget::eventFilter(QObject *watched, QEvent *event)
{
    // 拦截剪切、复制、粘贴、撤销、重做快捷键
    if ((watched == this || watched == this->lineEdit())
        && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Paste)
            || keyEvent->matches(QKeySequence::Cut)
            || keyEvent->matches(QKeySequence::Copy)
            || keyEvent->matches(QKeySequence::Undo)
            || keyEvent->matches(QKeySequence::Redo)) {
            return true;
        }
        if (keyEvent->matches(QKeySequence::SelectAll)) {
            // DTK also publishes PRIMARY for SelectAll, even in password mode.
            // Select directly without going through its clipboard handling.
            lineEdit()->selectAll();
            return true;
        }
    }

    // 拦截鼠标中键粘贴（X11 PRIMARY selection，绕过 Ctrl+V 的拦截路径）
    if ((watched == this || watched == this->lineEdit())
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            return true;
        }
    }

    if (watched == lineEdit() && m_passwordVisible && echoMode() == QLineEdit::Normal
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonRelease)) {
        // Qt publishes PRIMARY after keyboard selection and mouse release.
        // Process these events in password mode so nothing is ever exported,
        // then restore the revealed rendering without discarding the selection.
        // Password mode also prevents recursion when the event is dispatched.
        QPointer<PasswordInputWidget> guard(this);
        setEchoMode(QLineEdit::Password);
        QCoreApplication::sendEvent(watched, event);
        // Submitting the form can clear or delete this widget synchronously.
        if (guard && m_passwordVisible) {
            setEchoMode(QLineEdit::Normal);
            updatePasswordTextMargins();
        }
        return true;
    }

    return DLineEdit::eventFilter(watched, event);
}

void PasswordInputWidget::clearUndoRedoStack()
{
    QLineEdit *le = lineEdit();
    if (!le) {
        return;
    }
    // setText() 会清空 undo/redo 历史，但会重置光标位置并再次触发信号，
    // 用 QSignalBlocker 避免递归，并恢复光标到原位置。
    QSignalBlocker blocker(le);
    const int pos = le->cursorPosition();
    le->setText(le->text());
    le->setCursorPosition(pos);
}
