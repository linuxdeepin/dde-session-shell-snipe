// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionbasewindow.h"
#include "constants.h"
#include "centertopwidget.h"

#include <QDebug>
#include <QResizeEvent>
#include <QApplication>

using namespace DDESESSIONCC;

const int kDefaultWidgetIndex = 0;

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
    , m_stackedLayout(nullptr)
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
    m_centerVLayout->invalidate();

    m_centerWidget = widget;
    widget->show();

    qCInfo(DDE_SHELL) << "centerWidgetHeight: " << widget->height() << "centerWidgetTopSpacerHeight: " << spacerHeight;

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

    const int centerTopHeight = qMax(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT), m_centerTopWidget->sizeHint().height());
    m_centerTopFrame->setFixedHeight(centerTopHeight);
}

void SessionBaseWindow::initUI()
{
    //整理代码顺序，让子部件层级清晰明了,
    //同时方便计算中间区域的大小,使用QFrame替换了QScrollArea
    m_centerTopLayout = new QHBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#else
    m_centerTopLayout->setMargin(0);
#endif
    m_centerTopLayout->setSpacing(0);

    m_centerTopFrame = new QFrame;
    m_centerTopFrame->setAccessibleName("CenterTopFrame");
    m_centerTopFrame->setLayout(m_centerTopLayout);
    m_centerTopFrame->setFixedHeight(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT));
    m_centerTopFrame->setAutoFillBackground(false);

    m_centerLayout = new QHBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_centerLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_centerLayout->setMargin(0);
#endif
    m_centerLayout->setSpacing(0);

    m_centerVLayout = new QVBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_centerVLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_centerVLayout->setMargin(0);
#endif
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_leftBottomLayout->setContentsMargins(0, 0, 0, 0);
    m_centerBottomLayout->setContentsMargins(0, 0, 0, 0);
    m_rightBottomLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_leftBottomLayout->setMargin(0);
    m_centerBottomLayout->setMargin(0);
    m_rightBottomLayout->setMargin(0);
#endif

    m_leftBottomLayout->setSpacing(0);
    m_centerBottomLayout->setSpacing(0);
    m_rightBottomLayout->setSpacing(0);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bottomLayout->setContentsMargins(0, 0, 0, 0);
#else
    bottomLayout->setMargin(0);
#endif
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

    QWidget *basewindow = new QWidget(this);
    basewindow->setLayout(m_mainLayout);

    m_stackedLayout = new QStackedLayout;
    m_stackedLayout->setStackingMode(QStackedLayout::StackOne);
    m_stackedLayout->insertWidget(kDefaultWidgetIndex, basewindow);

    setLayout(m_stackedLayout);
}

void SessionBaseWindow::hideStackedWidgets()
{
    // disable and hide all widgets in stackedLayout
    int widCount = m_stackedLayout->count();
    if (widCount) {
        for (int index = 0; index < widCount; index++) {
            QWidget *tmp = m_stackedLayout->widget(index);
            // widgets are not members, so maybe it would be delete somewhere
            if (tmp) {
                tmp->hide();
                tmp->setDisabled(true);
            }
        }
    }
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

void SessionBaseWindow::setFullManagedLoginWidget(QWidget *wid)
{
    // set wid to nullptr, and the plugin ui would not be show
    m_pluginWidgets.insert(PluginWidgetIndex::FullManagedPlugin, wid);
    // 如果存在全托管，设为当前界面
    m_stackedLayout->setCurrentIndex(m_stackedLayout->addWidget(wid));
}

// for some widgets used by both plugin and default UI
void SessionBaseWindow::showControlFrame(QWidget *wid)
{
    if (!wid) {
        return;
    }

    hideStackedWidgets();

    // no need to record index
    // just insert the widget if it's isolated
    int widIndex = m_stackedLayout->indexOf(wid);
    if (widIndex < 0) {
        m_stackedLayout->addWidget(wid);
        connect(wid, &QWidget::destroyed, this, [&] {
            m_stackedLayout->removeWidget(wid);
        });
    }

    m_stackedLayout->setCurrentIndex(m_stackedLayout->indexOf(wid));
    wid->show();
    wid->setDisabled(false);
}

void SessionBaseWindow::showPasswdFrame()
{
    // plugin passwd frame first
    auto it = m_pluginWidgets.find(PluginWidgetIndex::FullManagedPlugin);
    if (it != m_pluginWidgets.end()) {
        QWidget *pluginWid = it.value();
        if (pluginWid) {
            showControlFrame(pluginWid);
            return;
        }
    }

    showDefaultFrame();
}

// userframe will be part or default widget
// just show default page
void SessionBaseWindow::showDefaultFrame()
{
    hideStackedWidgets();

    QWidget *defaultWid = m_stackedLayout->widget(kDefaultWidgetIndex);
    if (defaultWid) {
        defaultWid->setDisabled(false);
        defaultWid->show();
        m_stackedLayout->setCurrentIndex(kDefaultWidgetIndex);
    }
}

void SessionBaseWindow::resizeEvent(QResizeEvent *event)
{
    if (!m_centerTopWidget || !m_centerTopFrame || !m_mainLayout)
        return;

    const int margin = calcCurrentHeight(LOCK_CONTENT_CENTER_LAYOUT_MARGIN);
    m_mainLayout->setContentsMargins(0, margin, 0, margin);

    const int centerTopHeight = qMax(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT), m_centerTopWidget->sizeHint().height());
    m_centerTopFrame->setFixedSize(event->size().width(), centerTopHeight);

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

int SessionBaseWindow::calcCurrentHeight(int height) const
{
    int h = static_cast<int>(((double) height / (double) BASE_SCREEN_HEIGHT) * topLevelWidget()->geometry().height());
    return qMin(h, height);
}

int SessionBaseWindow::calcTopSpacing(int authWidgetTopSpacing) const
{
    if (!m_centerTopWidget)
        return qMax(0, authWidgetTopSpacing - calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT));

    const int topWidgetHeight = qMax(calcCurrentHeight(LOCK_CONTENT_TOP_WIDGET_HEIGHT), m_centerTopWidget->sizeHint().height());
    return qMax(0, authWidgetTopSpacing - topWidgetHeight);
}
