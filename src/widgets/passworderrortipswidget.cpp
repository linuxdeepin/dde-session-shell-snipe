// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later


#include "passworderrortipswidget.h"

#include <DFontSizeManager>

#include "QVBoxLayout"
#include "QHBoxLayout"
#include "QEvent"
#include "QMouseEvent"

#include "dpalette.h"
#include "dlabel.h"

const int MaxWidth = 402;
const int MinWidth = 140;
const int MaxHeight = 154;

PasswordErrorTipsWidget::PasswordErrorTipsWidget(QWidget *parent)
    : QWidget(parent)
    , m_tipLabel(new DLabel(this))
    , m_showMore(new DLabel(this))
    , m_detailTextEdit(new DTextEdit(this))
    , m_tipsWidget(new QWidget(this))
    , m_errorMsgNum(0)
{
    setAttribute(Qt::WA_TranslucentBackground); // 设置窗口背景透明
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip);

    initUi();

    m_showMore->installEventFilter(this);
    m_tipLabel->installEventFilter(this);
}

void PasswordErrorTipsWidget::initUi()
{
    QVBoxLayout* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(10, 5, 10, 5);
    mainLay->setSpacing(0);

    QHBoxLayout* hlay = new QHBoxLayout(m_tipsWidget);
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->setSpacing(5);
    hlay->addWidget(m_tipLabel, 0, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);
    DFontSizeManager::instance()->bind(m_tipLabel, DFontSizeManager::T6);
    m_tipLabel->setForegroundRole(DPalette::TextWarning);

    QPixmap pixmap;
    pixmap.load(":img/warning.svg");
    m_showMore->setPixmap(pixmap);
    m_showMore->setVisible(false);

    hlay->addWidget(m_showMore, 0, Qt::AlignmentFlag::AlignCenter);
    mainLay->addWidget(m_tipsWidget, 0, Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);

    m_detailTextEdit->hide();
    QFont detailFont;
    detailFont.setFamily("Noto Sans CJK SC-Thin");
    detailFont.setWeight(QFont::ExtraLight);
    QPalette palette = m_detailTextEdit->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    m_detailTextEdit->setPalette(palette);

    DFontSizeManager::instance()->bind(m_detailTextEdit, DFontSizeManager::T6);
    m_detailTextEdit->setAttribute(Qt::WA_TranslucentBackground, false);
    m_detailTextEdit->setFixedWidth(MaxWidth);
    m_detailTextEdit->setFont(detailFont);
    m_detailTextEdit->setFrameShape(QFrame::Shape::NoFrame);
    m_detailTextEdit->setReadOnly(true);
    m_detailTextEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    m_detailTextEdit->setLineWrapMode(QTextEdit::LineWrapMode::WidgetWidth);
    m_detailTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    m_detailTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    m_detailTextEdit->setAttribute(Qt::WA_TranslucentBackground, true);

    mainLay->addWidget(m_detailTextEdit, 0, Qt::AlignmentFlag::AlignLeft);
}

bool PasswordErrorTipsWidget::eventFilter(QObject *watch, QEvent *event)
{
    if (watch->isWidgetType() && event->type() == QEvent::MouseButtonPress
    && (qobject_cast<QLabel *>(watch) == m_tipLabel || qobject_cast<QLabel *>(watch) == m_showMore)) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (m_detailTextEdit->toPlainText().isEmpty()) {
                return QObject::eventFilter(watch, event);
            }
            m_detailTextEdit->setText(m_detailTextEdit->toPlainText());
            m_detailTextEdit->setVisible(!m_detailTextEdit->isVisible());
            QSizeF docSize = m_detailTextEdit->textCursor().document()->size();
            int height = docSize.height() > MaxHeight ? MaxHeight : docSize.height();
            int width = 0;
            if (docSize.width() <= MinWidth) {
                width = MinWidth;
            } else if (docSize.width() <= MaxWidth) {
                width = docSize.width();
            } else {
                width = MaxWidth;
            }
            m_detailTextEdit->setFixedSize(width, height);
            adjustSize();
        }
    }
    return QObject::eventFilter(watch, event);
}

void PasswordErrorTipsWidget::setErrDetailVisible(bool isVisible)
{
    m_detailTextEdit->setVisible(isVisible);
}

void PasswordErrorTipsWidget::addErrorDetailMsg(const QString& msg)
{
    m_errorMsgNum ++;
    m_detailTextEdit->append(QString("%1. ").arg(m_errorMsgNum) + msg);
    m_showMore->setVisible(true);
}

void PasswordErrorTipsWidget::setErrorTips(const QString &tips)
{
    m_tipLabel->setText(tips);
}

void PasswordErrorTipsWidget::paintEvent(QPaintEvent *event)
{
    // 创建绘图对象
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 抗锯齿

    // 设置画笔和画刷
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(164, 166, 189, 255)); // 半透明灰色

    // 绘制圆角矩形
    QRectF rect = QRectF(0, 0, width(), height());
    painter.drawRoundedRect(rect, 10, 8);

    QWidget::paintEvent(event);
}

void PasswordErrorTipsWidget::reset()
{
    m_errorMsgNum = 0;
    m_detailTextEdit->clear();
    m_detailTextEdit->hide();
    m_tipLabel->clear();
}

bool PasswordErrorTipsWidget::hasErrorTips()
{
    return !m_tipLabel->text().isEmpty();
}
