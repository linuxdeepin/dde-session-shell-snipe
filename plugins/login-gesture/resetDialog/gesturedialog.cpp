// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gesturedialog.h"
#include "gesturepannel.h"
#include "modulewidget.h"
#include "resetpatterncontroller.h"
#include "waypointmodel.h"
#include "translastiondoc.h"

#include <DTitlebar>
#include <DIconButton>
#include <DPlatformWindowHandle>
#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QStackedLayout>
#include <QMouseEvent>
#include <QSvgRenderer>
#include <QPainter>
#include <QDesktopWidget>
#include <QApplication>

using namespace gestureSetting;
using namespace gestureLogin;

GestureDialog::GestureDialog(ResetPatternController *worker, QWidget *parent)
    : QWidget(parent)
    , m_handle(new DPlatformWindowHandle(this))
    , m_titleBar(new QWidget(this))
    , m_mainContent(new QWidget(this))
    , m_mainLayout(new QStackedLayout(m_mainContent))
    , m_authWidget(new ModuleWidget(m_mainContent))
    , m_successWidget(new QWidget(m_mainContent))
    , m_confirmButton(nullptr)
    , m_worker(worker)
{
    initUi();
    initConnection();
}

GestureDialog::~GestureDialog() {}

void GestureDialog::initUi()
{
    setAccessibleName("ResetGesturePage");

    m_handle->setEnableSystemMove(false);
    m_handle->setShadowOffset(QPoint(5, 5));
    m_handle->setShadowRadius(20);

    m_titleBar->setFixedHeight(50 * qApp->devicePixelRatio());
    QHBoxLayout *layoutTitle = new QHBoxLayout(m_titleBar);
    layoutTitle->setContentsMargins(10, 0, 10, 0);
    layoutTitle->setSpacing(0);
    // 警告图标
    layoutTitle->addWidget(createTitleIcon(QIcon::fromTheme("dialog-warning")));
    layoutTitle->addStretch();
    // 右侧关闭按钮
    QLabel *closeButton = createTitleIcon(QIcon::fromTheme("dialog-close"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setObjectName("close_button");
    closeButton->installEventFilter(this);
    layoutTitle->addWidget(closeButton);

    auto *mainContentLayout = new QVBoxLayout(this);
    mainContentLayout->setContentsMargins(0, 0, 0, 0);
    mainContentLayout->addWidget(m_titleBar, 0, Qt::AlignTop);
    mainContentLayout->addSpacing(30);
    mainContentLayout->addWidget(m_mainContent);
    mainContentLayout->addWidget(createCancelButton(), 0, Qt::AlignBottom);

    m_mainContent->setMinimumHeight(605);
    m_mainLayout->addWidget(m_authWidget);
    createSuccessWidget();
    m_mainLayout->addWidget(m_successWidget);
    m_mainLayout->setCurrentWidget(m_authWidget);
}

void GestureDialog::initConnection()
{
    // token输入完成并且已经调用父进程写入
    connect(m_worker, &ResetPatternController::inputFinished, this, &GestureDialog::onInputFinished);
    connect(m_confirmButton, &QPushButton::clicked, this, &GestureDialog::close);
    m_titleBar->installEventFilter(this);
}

QWidget* GestureDialog::createCancelButton()
{
    auto *cancelWidget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(cancelWidget);
    layout->setContentsMargins(10, 0, 10, 10);
    m_confirmButton = new QPushButton(TranslastionDoc::instance()->getDoc(DocIndex::Cancel), cancelWidget);
    m_confirmButton->setAttribute(Qt::WA_NoMousePropagation);
    layout->addWidget(m_confirmButton);

    return cancelWidget;
}

QLabel *GestureDialog::createTitleIcon(const QIcon &icon)
{
    QLabel *label = new QLabel(m_titleBar);
    QPixmap pixmap = icon.pixmap(QSize(40, 40) * qApp->devicePixelRatio());
    pixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    label->setFixedHeight(40 * qApp->devicePixelRatio());
    label->setPixmap(pixmap);
    return label;
}

void GestureDialog::createSuccessWidget()
{
    TranslastionDoc *transDoc = TranslastionDoc::instance();
    auto *resultLayout = new QVBoxLayout(m_successWidget);
    resultLayout->setContentsMargins(0, 8, 0, 0);

    QLabel *labelTitle = new QLabel(m_successWidget);
    DFontSizeManager::instance()->bind(labelTitle, DFontSizeManager::T3);
    labelTitle->setAlignment(Qt::AlignCenter);
    labelTitle->setText(transDoc->getDoc(DocIndex::SetPasswd));
    resultLayout->addWidget(labelTitle);

    auto *indicatorWidget = new QWidget(m_successWidget);
    indicatorWidget->setFixedHeight(128);
    indicatorWidget->setObjectName("indicatorSuccess");
    indicatorWidget->installEventFilter(this);

    resultLayout->addSpacing(10);
    resultLayout->addWidget(indicatorWidget);

    auto *labelSuccess = new QLabel(transDoc->getDoc(DocIndex::EnrollDone), m_successWidget);
    DFontSizeManager::instance()->bind(labelSuccess, DFontSizeManager::T5);
    labelSuccess->setAlignment(Qt::AlignHCenter);
    resultLayout->setSpacing(35);
    resultLayout->addWidget(labelSuccess);
    resultLayout->addStretch();
}

bool GestureDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons().testFlag(Qt::LeftButton))) {
                m_dragPosition = mapFromGlobal(mouseEvent->globalPos());
            }
        } break;
        case QEvent::MouseButtonRelease: {
            m_dragPosition = QPoint();
        } break;
        case QEvent::MouseMove: {
            // 如果按下的点为空，则不做任何处理
            if (m_dragPosition.isNull())
                break;

            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            int x = mouseEvent->globalX() - m_dragPosition.x();
            int y = mouseEvent->globalY() - m_dragPosition.y();

            move(x, y);
        } break;
        default: break;
        }
    } else if (watched->isWidgetType()) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget) {
            if (widget->objectName() == "indicatorSuccess" && event->type() == QEvent::Paint) {
                // 绘制图标
                QSvgRenderer renderer(QString(":/success.svg"));
                QPixmap pixmap(QSize(128, 128) * qApp->devicePixelRatio());
                pixmap.fill(Qt::transparent);
                QPainter painter;
                painter.begin(&pixmap);
                painter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                renderer.render(&painter);
                painter.end();
                QSize pixmapSize(widget->height(), widget->height());
                QRect pixmapRect((widget->width() - pixmapSize.width()) / 2,
                                 (widget->height() - pixmapSize.height()) / 2,
                                 widget->height(), widget->height());
                painter.begin(widget);
                painter.drawPixmap(pixmapRect, pixmap);
                painter.end();
            } else if (widget->objectName() == "close_button"
                        && event->type() == QEvent::MouseButtonRelease) {
                close();
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GestureDialog::onInputFinished()
{
    // 两次校验通过
    m_mainLayout->setCurrentWidget(m_successWidget);
    m_confirmButton->setText(TranslastionDoc::instance()->getDoc(DocIndex::Ok));
}

ResetPatternController *Manager::m_controller = nullptr;

Manager::Manager(int old_file_id, int new_file_id, QObject *parent)
    : QObject (parent)
    , m_gestureDialog(nullptr)
    , m_old_file_id (old_file_id)
    , m_new_file_id(new_file_id)
{
}

Manager::~Manager()
{
    free();
    m_gestureDialog->deleteLater();
}

void Manager::start()
{
    if (m_gestureDialog)
        return;

    if (!m_controller) {
        m_controller = new ResetPatternController;
        if (m_old_file_id >= 0)
            m_controller->setOldPasswordFileId(m_old_file_id);

        if (m_new_file_id >= 0)
            m_controller->setNewPasswordFileId(m_new_file_id);

        m_controller->start();
    }

    m_gestureDialog = new GestureDialog(m_controller);

    QDesktopWidget *desktopWidget = QApplication::desktop();
    m_gestureDialog->setFixedSize(380, 566);
    int x = (desktopWidget->size().width() - m_gestureDialog->width()) / 2;
    int y = (desktopWidget->size().height() - m_gestureDialog->height()) / 2;
    m_gestureDialog->move(x, y);
    m_gestureDialog->setWindowFlags(m_gestureDialog->windowFlags() | Qt::FramelessWindowHint);
    m_gestureDialog->show();
}

void Manager::exit(int retCode)
{
    free();
    qDebug() << "exit code" << retCode;
    qApp->quit();
}

void Manager::free()
{
    qDebug() << "free manager";
    if (m_controller) {
        m_controller->close();
        m_controller->deleteLater();
    }
}
