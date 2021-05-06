#include "sessionbasewindow.h"

#include <QDebug>
#include <QResizeEvent>
#include <QApplication>

SessionBaseWindow::SessionBaseWindow(QWidget *parent)
    : QFrame(parent)
    , m_centerTopFrame(nullptr)
    , m_centerFrame(nullptr)
    , m_bottomFrame(nullptr)
    , m_mainLayou(nullptr)
    , m_centerTopLayout(nullptr)
    , m_centerLayout(nullptr)
    , m_leftBottomLayout(nullptr)
    , m_centerBottomLayout(nullptr)
    , m_rightBottomLayout(nullptr)
    , m_centerTopWidget(nullptr)
    , m_centerWidget(nullptr)
    , m_leftBottomWidget(nullptr)
    , m_centerBottomWidget(nullptr)
    , m_rightBottomWidget(nullptr)
{
    initUI();
}

void SessionBaseWindow::setLeftBottomWidget(QWidget *const widget)
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

void SessionBaseWindow::setRightBottomWidget(QWidget *const widget)
{
    if (m_rightBottomWidget != nullptr) {
        m_rightBottomLayout->removeWidget(m_rightBottomWidget);
    }

    m_rightBottomLayout->addWidget(widget, 0, Qt::AlignBottom);
    m_rightBottomWidget = widget;
}

void SessionBaseWindow::setCenterContent(QWidget *const widget)
{
    if (m_centerWidget == widget)
        return;

    if (m_centerWidget != nullptr) {
        m_centerLayout->removeWidget(m_centerWidget);
        m_centerWidget->hide();
        m_centerWidget->setParent(nullptr);
    }
    m_centerWidget = widget;

    //Minimum 布局中部件大小是最小值并且是足够大的。而部件允许扩展，并不一定要扩展，但是不能比缺省大小更小
    m_centerWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_centerLayout->addWidget(m_centerWidget, 0, Qt::AlignCenter);
    m_centerWidget->setFocus();
    m_centerWidget->show();
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
    m_centerTopFrame->setLayout(m_centerTopLayout);
    m_centerTopFrame->setFixedHeight(132);
    m_centerTopFrame->setAutoFillBackground(false);

    m_centerLayout = new QHBoxLayout;
    m_centerLayout->setMargin(0);
    m_centerLayout->setSpacing(0);

    m_centerFrame = new QFrame;
    m_centerFrame->setLayout(m_centerLayout);
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
    bottomLayout->addLayout(m_leftBottomLayout, 1);
    bottomLayout->addLayout(m_centerBottomLayout, 3);
    bottomLayout->addLayout(m_rightBottomLayout, 1);

    m_bottomFrame = new QFrame;
    m_bottomFrame->setLayout(bottomLayout);
    m_bottomFrame->setFixedHeight(132);
    m_bottomFrame->setAutoFillBackground(false);

    m_mainLayou = new QVBoxLayout;
    m_mainLayou->setContentsMargins(0, 33, 0, 33);
    m_mainLayou->setSpacing(0);
    m_mainLayou->addWidget(m_centerTopFrame);
    m_mainLayou->addWidget(m_centerFrame);
    m_mainLayou->addWidget(m_bottomFrame);

    setLayout(m_mainLayou);
}

QSize SessionBaseWindow::getCenterContentSize()
{
    //计算中间区域的大小
    int w = width() - m_mainLayou->contentsMargins().left() - m_mainLayou->contentsMargins().right();
    int h = height() - m_centerTopFrame->height() - m_bottomFrame->height() - m_mainLayou->contentsMargins().top() - m_mainLayou->contentsMargins().bottom();
    return QSize(w, h);
}

