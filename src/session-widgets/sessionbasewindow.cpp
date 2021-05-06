#include "sessionbasewindow.h"

#include <QDebug>
#include <QScrollArea>
#include <QResizeEvent>
#include <QApplication>

SessionBaseWindow::SessionBaseWindow(QWidget *parent)
    : QFrame(parent)
    , m_centerWidget(nullptr)
    , m_leftBottomWidget(nullptr)
    , m_rightBottomWidget(nullptr)
    , m_centerTopWidget(nullptr)
    , m_centerBottomWidget(nullptr)
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
    if (m_centerBottomLayout != nullptr) {
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

    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_centerLayout->addWidget(widget);
    m_centerWidget = widget;

    widget->setFocus();
    widget->show();
}

void SessionBaseWindow::setCenterTopWidget(QWidget *const widget)
{
    if (m_centerTopLayout != nullptr) {
        m_centerTopLayout->removeWidget(m_centerTopWidget);
    }
    m_centerTopLayout->addStretch();
    m_centerTopLayout->addWidget(widget, 0, Qt::AlignTop);
    m_centerTopLayout->addStretch();
    m_centerTopWidget = widget;
}

void SessionBaseWindow::initUI()
{
    m_mainLayou = new QVBoxLayout;

    m_centerLayout = new QHBoxLayout;
    m_leftBottomLayout = new QHBoxLayout;
    m_rightBottomLayout = new QHBoxLayout;
    m_centerTopLayout = new QHBoxLayout;
    m_centerBottomLayout = new QHBoxLayout;

    m_centerLayout->setMargin(0);
    m_centerLayout->setSpacing(0);

    m_leftBottomLayout->setMargin(0);
    m_leftBottomLayout->setSpacing(0);

    m_rightBottomLayout->setMargin(0);
    m_rightBottomLayout->setSpacing(0);

    m_centerTopLayout->setMargin(0);
    m_centerTopLayout->setSpacing(0);

    m_centerBottomLayout->setMargin(0);
    m_centerBottomLayout->setSpacing(0);

    QFrame *bottomWidget = new QFrame;
    m_scrollArea = new QScrollArea;
    QWidget *centerWidget = new QWidget;
    centerWidget->setLayout(m_centerLayout);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFocusPolicy(Qt::NoFocus);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidget(centerWidget);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    centerWidget->setAutoFillBackground(false);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setMargin(0);
    bottomLayout->setSpacing(0);

    bottomLayout->addLayout(m_leftBottomLayout);
    bottomLayout->addLayout(m_centerBottomLayout);
    bottomLayout->addLayout(m_rightBottomLayout);
    bottomLayout->setStretch(0, 1);
    bottomLayout->setStretch(1, 3);
    bottomLayout->setStretch(2, 1);

    bottomWidget->setLayout(bottomLayout);
    bottomWidget->setFixedHeight(132);

    m_mainLayou->setContentsMargins(0, 33, 0, 33);
    m_mainLayou->setSpacing(0);

    QFrame *topCenterWidget = new QFrame;
    topCenterWidget->setLayout(m_centerTopLayout);
    topCenterWidget->setFixedHeight(132);

    m_mainLayou->addWidget(topCenterWidget, 0, Qt::AlignTop);
    m_mainLayou->addWidget(m_scrollArea);
    m_mainLayou->addWidget(bottomWidget, 0, Qt::AlignBottom);

    setLayout(m_mainLayou);
}

