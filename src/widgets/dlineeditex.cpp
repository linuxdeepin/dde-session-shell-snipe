// SPDX-FileCopyrightText: 2019 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dlineeditex.h"

#include <DFontSizeManager>

#include <QGuiApplication>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QKeyEvent>
#include <QKeySequence>

LoadSlider::LoadSlider(QWidget *parent)
    : QWidget(parent)
    , m_loadSliderColor(Qt::gray)
{
}

void LoadSlider::setLoadSliderColor(const QColor &color)
{
    m_loadSliderColor = color;
}

void LoadSlider::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QLinearGradient grad(0, height() / 2, width(), height() / 2);
    grad.setColorAt(0.0, Qt::transparent);
    grad.setColorAt(1.0, m_loadSliderColor);
    painter.fillRect(0, 0, width(), height() - 1, grad);

    QWidget::paintEvent(event);
}

DLineEditEx::DLineEditEx(QWidget *parent)
    : DLineEdit(parent)
    , m_loadSlider(new LoadSlider(this))
    , m_animation(new QPropertyAnimation(m_loadSlider, "pos", m_loadSlider))
    , m_enableTableKeyEvent(false)
    , m_secureInputEnabled(false)
{
    setObjectName(QStringLiteral("DLineEditEx"));
    setAccessibleName(QStringLiteral("DLineEditEx"));

    initAnimation();

#if 0 // FIXME:不生效,改为在eventFilter过滤InputMethodQuery事件
    // 在锁屏或登录界面，输入类型的控件不要唤出输入法
    setAttribute(Qt::WA_InputMethodEnabled, false);
    this->lineEdit()->setAttribute(Qt::WA_InputMethodEnabled, false);
#endif
    this->lineEdit()->installEventFilter(this);
    this->installEventFilter(this);

    connect(qGuiApp, &QGuiApplication::fontChanged, this, &DLineEditEx::setPlaceholderTextFont);
}

/**
 * @brief 初始化动画
 */
void DLineEditEx::initAnimation()
{
    m_loadSlider->setGeometry(0, -40, 40, height());
    m_loadSlider->setLoadSliderColor(QColor(255, 255, 255, 90));
    m_loadSlider->setAccessibleName("LoadSlider");
    m_animation->setDuration(1000);
    m_animation->setLoopCount(-1);
    m_animation->setEasingCurve(QEasingCurve::Linear);
    m_animation->targetObject();
}

/**
 * @brief 设置字体
 * @param font
 */
void DLineEditEx::setPlaceholderTextFont(const QFont &font)
{
    const QString &text = lineEdit()->placeholderText();
    QFont fontTmp = font;
    // 计算有效文本宽度（扣除 textMargins + layout contentsMargins）
    QMargins textMargins = lineEdit()->textMargins();
    QMargins layoutMargins(0, 0, 0, 0);
    if (auto *layout = lineEdit()->layout()) {
        layoutMargins = layout->contentsMargins();
    }
    int availWidth = width() - textMargins.left() - textMargins.right()
                   - layoutMargins.left() - layoutMargins.right();
    while (QFontMetrics(fontTmp).boundingRect(text).width() > availWidth) {
        // 防止陷入死循环，setPointSize当参数为负数的时候设置不生效
        // TODO: font()获取问题，需要继续调查下为啥font为啥会出问题，但是这里也需要增加跳出死循环的条件
        qDebug() << "Password line edit placeholder text width : " << QFontMetrics(fontTmp).boundingRect(text).width() << " line edit width : " << width();
        if (fontTmp.pointSize() <= 1) {
            qWarning() << "Password line edit font size" << font.pointSize() << fontTmp.pointSize();
            return;
        }
        fontTmp.setPointSize(fontTmp.pointSize() - 1);
    }
    setFont(fontTmp);
}

/**
 * @brief 显示动画
 */
void DLineEditEx::startAnimation()
{
    if (m_animation->state() == QAbstractAnimation::Running) {
        return;
    }
    m_loadSlider->show();
    m_loadSlider->resize(40, height());
    // 动画块距离右侧的距离10px结束
    int endX = (width() - m_loadSlider->width() - 10) > 0 ? width() - m_loadSlider->width() - 10 : width();
    m_animation->setStartValue(QPoint(0, 0));
    m_animation->setEndValue(QPoint(endX, 0));
    m_animation->start();
}

/**
 * @brief 隐藏动画
 */
void DLineEditEx::stopAnimation()
{
    if (m_animation->state() == QAbstractAnimation::Stopped) {
        return;
    }
    m_loadSlider->hide();
    m_animation->stop();
}

void DLineEditEx::setEnableTableKeyEvent(bool enable)
{
    if (m_enableTableKeyEvent == enable)
        return;

    m_enableTableKeyEvent = enable;
}

void DLineEditEx::setSecureInputEnabled(bool enable)
{
    if (m_secureInputEnabled == enable)
        return;

    m_secureInputEnabled = enable;
}

/**
 * @brief 重写 QLineEdit paintEvent 函数，实现当文本设置居中后，holderText 仍然显示的需求
 *
 * @param event
 */
void DLineEditEx::paintEvent(QPaintEvent *event)
{
    setPlaceholderTextFont(font());
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
        option.setWrapMode(QTextOption::NoWrap);
        // 使用 elidedText 确保文本过长时在右侧显示省略号，而不是换行
        QFontMetrics fm(pa.font());
        const QString &placeholderText = lineEdit()->placeholderText();
        // 读取内部 QLineEdit 的 textMargins 和 layout contentsMargins，计算扣除图标区域后的有效文本 rect
        // 避免占位文本与右侧图标（大写状态、密码显示、密码提示等）重叠
        QRect leRect = lineEdit()->geometry();
        QMargins textMargins = lineEdit()->textMargins();
        QMargins layoutMargins(0, 0, 0, 0);
        if (auto *layout = lineEdit()->layout()) {
            layoutMargins = layout->contentsMargins();
        }
        int leftOffset = textMargins.left() + layoutMargins.left();
        int rightOffset = textMargins.right() + layoutMargins.right();
        QRect textRect(leRect.x() + leftOffset, rect().y(),
                       leRect.width() - leftOffset - rightOffset,
                       rect().height());
        QString elidedText = fm.elidedText(placeholderText, Qt::ElideRight, textRect.width());
        pa.drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, elidedText);

        // 当文本被省略时，设置 tooltip 显示完整文本
        if (elidedText != placeholderText) {
            setToolTip(placeholderText);
        } else {
            setToolTip(QString());
        }
    }
    QWidget::paintEvent(event);
}

bool DLineEditEx::eventFilter(QObject *watched, QEvent *event)
{
    // 禁止输入法
    if ((watched == this || watched == this->lineEdit())
        && event->type() == QEvent::InputMethodQuery) {
        return true;
    } else if ((watched == this || watched == this->lineEdit())
               && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (m_secureInputEnabled
            && (keyEvent->matches(QKeySequence::Cut)
                || keyEvent->matches(QKeySequence::Copy)
                || keyEvent->matches(QKeySequence::Paste)
                || keyEvent->matches(QKeySequence::Undo)
                || keyEvent->matches(QKeySequence::Redo))) {
            return true;
        }

        if (m_enableTableKeyEvent
            && keyEvent->key() == Qt::Key_Tab
            && keyEvent->modifiers() == Qt::NoModifier
            && !this->text().isEmpty()) {
            emit this->returnPressed();
            return true;
        }
    }

    return DLineEdit::eventFilter(watched, event);
}
