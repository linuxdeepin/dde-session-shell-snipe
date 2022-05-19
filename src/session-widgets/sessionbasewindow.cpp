#include "sessionbasewindow.h"
#include "constants.h"

#include <QDebug>
#include <QResizeEvent>
#include <QApplication>

using namespace DDESESSIONCC;

SessionBaseWindow::SessionBaseWindow(QWidget *parent)
    : QFrame(parent)
    , m_centerTopFrame(nullptr)
    , m_centerFrame(nullptr)
    , m_bottomFrame(nullptr)
    , m_mainLayout(nullptr)
    , m_centerTopLayout(nullptr)
    , m_centerLayout(nullptr)
    , m_centerVLayout(nullptr)
    , m_leftBottomLayout(nullptr)
    , m_centerBottomLayout(nullptr)
    , m_rightBottomLayout(nullptr)
    , m_centerTopWidget(nullptr)
    , m_centerWidget(nullptr)
    , m_leftBottomWidget(nullptr)
    , m_centerBottomWidget(nullptr)
    , m_rightBottomWidget(nullptr)
    , m_centerSpacerItem(new QSpacerItem(0, 0))
{
    initUI();
}

void SessionBaseWindow::setLeftBottomWidget(QWidget * const widget)
{
    if (m_leftBottomWidget != nullptr) {
        m_leftBottomLayout->removeWidget(m_leftBottomWidget);
    }

    m_leftBottomLayout->addWidget(widget, 0, Qt::AlignBottom);
    m_leftBottomWidget = widget;
}

void SessionBaseWindow::setCenterBottomWidget(QWidget *const widget)
{
    if (m_centerBottomWidget != nullptr) {
        m_centerBottomLayout->removeWidget(m_centerBottomWidget);
    }
    m_centerBottomLayout->addWidget(widget, 0, Qt::AlignCenter);
    m_centerBottomWidget = widget;
}

void SessionBaseWindow::setRightBottomWidget(QWidget * const widget)
{
    if (m_rightBottomWidget != nullptr) {
        m_rightBottomLayout->removeWidget(m_rightBottomWidget);
    }

    m_rightBottomLayout->addWidget(widget, 0, Qt::AlignBottom);
    m_rightBottomWidget = widget;
}

void SessionBaseWindow::setCenterContent(QWidget * const widget, int stretch, Qt::Alignment align, int spacerHeight)
{
    if (!widget || m_centerWidget == widget) {
        return;
    }
    if (m_centerWidget) {
        m_centerLayout->removeWidget(m_centerWidget);
        m_centerWidget->hide();
    }
    m_centerLayout->addWidget(widget, stretch, align);
    m_centerSpacerItem->changeSize(0, spacerHeight);

    m_centerWidget = widget;
    widget->show();

    setFocusProxy(widget);
    setFocus();
}

void SessionBaseWindow::setCenterTopWidget(QWidget *const widget)
{
    if (m_centerTopWidget != nullptr) {
        m_centerTopLayout->removeWidget(m_centerTopWidget);
    }
    m_centerTopLayout->addStretch();
    m_centerTopLayout->addWidget(widget, 0, Qt::AlignTop);
    m_centerTopLayout->addStretch();
    m_centerTopWidget = widget;
}

void SessionBaseWindow::initUI()
{
    //整理代码顺序，让子部件层级清晰明了,
    //同时方便计算中间区域的大小,使用QFrame替换了QScrollArea
    m_centerTopLayout = new QHBoxLayout;
    m_centerTopLayout->setMargin(0);
    m_centerTopLayout->setSpacing(0);

    m_centerTopFrame = new QFrame;
    m_centerTopFrame->setAccessibleName("CenterTopFrame");
    m_centerTopFrame->setLayout(m_centerTopLayout);
    m_centerTopFrame->setFixedHeight(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT));
    m_centerTopFrame->setAutoFillBackground(false);

    m_centerLayout = new QHBoxLayout;
    m_centerLayout->setMargin(0);
    m_centerLayout->setSpacing(0);

    m_centerVLayout = new QVBoxLayout;
    m_centerVLayout->setMargin(0);
    m_centerVLayout->setSpacing(0);
    m_centerVLayout->addSpacerItem(m_centerSpacerItem);
    m_centerVLayout->addLayout(m_centerLayout);

    m_centerFrame = new QFrame;
    m_centerFrame->setAccessibleName("CenterFrame");
    m_centerFrame->setLayout(m_centerVLayout);
    m_centerFrame->setAutoFillBackground(false);

    m_leftBottomLayout = new QHBoxLayout;
    m_rightBottomLayout = new QHBoxLayout;
    m_centerBottomLayout = new QHBoxLayout;

    m_leftBottomLayout->setMargin(0);
    m_leftBottomLayout->setSpacing(0);
    m_centerBottomLayout->setMargin(0);
    m_centerBottomLayout->setSpacing(0);
    m_rightBottomLayout->setMargin(0);
    m_rightBottomLayout->setSpacing(0);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setMargin(0);
    bottomLayout->setSpacing(0);
    bottomLayout->addLayout(m_leftBottomLayout, 3);
    bottomLayout->addLayout(m_centerBottomLayout, 2);
    bottomLayout->addLayout(m_rightBottomLayout, 3);

    m_bottomFrame = new QFrame;
    m_bottomFrame->setAccessibleName("BottomFrame");
    m_bottomFrame->setLayout(bottomLayout);
    m_bottomFrame->setFixedHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT);
    m_bottomFrame->setAutoFillBackground(false);

    m_mainLayout = new QVBoxLayout;
    const int margin = calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN);
    m_mainLayout->setContentsMargins(0, margin, 0, margin);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_centerTopFrame);
    m_mainLayout->addWidget(m_centerFrame);
    m_mainLayout->addWidget(m_bottomFrame);

    setLayout(m_mainLayout);
}

QSize SessionBaseWindow::getCenterContentSize()
{
    //计算中间区域的大小
    int w = width() - m_mainLayout->contentsMargins().left() - m_mainLayout->contentsMargins().right();

    int h = height() - m_mainLayout->contentsMargins().top() - m_mainLayout->contentsMargins().bottom();
    if (m_centerTopFrame->isVisible()) {
        h = h - m_centerTopFrame->height();
    }
    if (m_bottomFrame->isVisible()) {
        h = h - m_bottomFrame->height();
    }

    return QSize(w, h);
}

void SessionBaseWindow::resizeEvent(QResizeEvent *event)
{
    const int margin = calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN);
    m_mainLayout->setContentsMargins(0, margin, 0, margin);
    m_centerTopFrame->setFixedHeight(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT));

    QFrame::resizeEvent(event);
}

void SessionBaseWindow::setTopFrameVisible(bool visible)
{
    m_centerTopFrame->setVisible(visible);
}

void SessionBaseWindow::setBottomFrameVisible(bool visible)
{
    m_bottomFrame->setVisible(visible);
}

int SessionBaseWindow::calcCurrentHeight(const int height)
{
    int h = static_cast<int>(((double) height / (double) BASE_SCREEN_HEIGHT) * topLevelWidget()->geometry().height());
    return qMin(h, height);
}
