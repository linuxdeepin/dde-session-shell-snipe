// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inhibitwarnview.h"

#include <DStyle>
#include <DFontSizeManager>
#include <DSpinner>
#include <DLabel>
#include <DToolTip>

#ifdef ENABLE_DSS_SNIPE
#include <DSGApplication>
#include <DUtil>
#include <DDBusSender>
#endif

#include <QHBoxLayout>

DWIDGET_USE_NAMESPACE

#ifdef ENABLE_DSS_SNIPE
#define AMDBUS_SERVICE "org.desktopspec.ApplicationManager1"
#define AMDBUS_PATH_APP_PREFIX "/org/desktopspec/ApplicationManager1"
#define AMDBUS_APP_INTERFACE "org.desktopspec.ApplicationManager1.Application"
#endif

const int ButtonWidth = 200;
const int ButtonHeight = 64;
const int FIXED_INHIBITOR_WIDTH = 328;
const QSize iconSize = QSize(24, 24);

InhibitorRow::InhibitorRow(const QString &who, const QString &why, const QIcon &icon, QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout;
    DLabel *whoLabel = new DLabel(who);
    whoLabel->setElideMode(Qt::ElideRight);

    DToolTip::setToolTipShowMode(whoLabel, DToolTip::ShowWhenElided);

    if (!icon.isNull()) {
        QLabel *iconLabel = new QLabel(this);
        QPixmap pixmap = icon.pixmap(topLevelWidget()->windowHandle(), QSize(48, 48));
        iconLabel->setPixmap(pixmap);
        iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        layout->addWidget(iconLabel);
    }

    layout->addWidget(whoLabel);
    layout->addStretch();
    this->setFixedHeight(ButtonHeight);
    this->setLayout(layout);
}

InhibitorRow::~InhibitorRow()
{

}

void InhibitorRow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 25));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawRoundedRect(this->rect(), 18, 18);
}

InhibitWarnView::InhibitWarnView(SessionBaseModel::PowerAction inhibitType, QWidget *parent)
    : WarningView(parent)
    , m_inhibitType(inhibitType)
    , m_acceptBtn(new InhibitButton(this))
    , m_cancelBtn(new InhibitButton(this))
    , m_bottomWidget(new QWidget(this))
    , m_showDelay(false)
{
    initUi();
    initConnection();
}

InhibitWarnView::~InhibitWarnView()
{

}

void InhibitWarnView::setInhibitorList(const QList<InhibitorData> &list)
{
    for (QWidget *widget : m_inhibitorPtrList) {
        m_inhibitorListLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_inhibitorPtrList.clear();

    for (const InhibitorData &inhibitor : list) {
        QIcon icon;

        if (inhibitor.icon.isEmpty()) {
            QString iconName;
#ifdef ENABLE_DSS_SNIPE
            do {
                const auto appId = Dtk::Core::DSGApplication::getId(inhibitor.pid);
                if (appId.isEmpty())
                    break;
                const auto amDBusAppPath = QString("%1/%2").arg(AMDBUS_PATH_APP_PREFIX, DUtil::escapeToObjectPath(appId));
                QDBusReply<QDBusVariant> reply = DDBusSender()
                    .service(AMDBUS_SERVICE)
                    .path(amDBusAppPath)
                    .interface(AMDBUS_APP_INTERFACE)
                    .property("Icons")
                    .get();
                if (reply.isValid()) {
                    auto ret = qdbus_cast<QMap<QString, QString>>(reply.value().variant());
                    iconName = ret.value("Desktop Entry");
                } else {
                    qCWarning(DDE_SHELL) << "Get icon error:" << reply.error().message();
                    break;
                }
            } while(0);
#endif

            if (iconName.isEmpty() && inhibitor.pid) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                QFileInfo executable_info(QFile::symLinkTarget(QString("/proc/%1/exe").arg(inhibitor.pid)));
#else
                QFileInfo executable_info(QFile::readLink(QString("/proc/%1/exe").arg(inhibitor.pid)));
#endif
                // 玲珑应用的exe指向的路径是容器内的路径，在容器外无法访问，因此不能判断文件是否存在
                QString iconName = executable_info.fileName();
            }

            icon = QIcon::fromTheme(iconName);
        } else {
            icon = QIcon::fromTheme(inhibitor.icon, QIcon::fromTheme("application-x-desktop"));
        }

        if (icon.isNull()) {
            icon = QIcon::fromTheme("application-x-desktop");
        }

        QWidget *inhibitorWidget = new InhibitorRow(inhibitor.who, inhibitor.why, icon, this);

        m_inhibitorPtrList.append(inhibitorWidget);
        m_inhibitorListLayout->addWidget(inhibitorWidget);
    }
}

void InhibitWarnView::setInhibitConfirmMessage(const QString &text, bool showLoading)
{
    m_confirmTextLabel->setText(text);

    if (showLoading && m_loading->width() != m_loading->height())
        m_loading->setFixedWidth(m_loading->height());
    m_loading->setVisible(showLoading);
    showLoading ? m_loading->start() : m_loading->stop();
}

void InhibitWarnView::setDelayView(bool showDelay)
{
    m_showDelay = showDelay;
}

bool InhibitWarnView::delayView() const
{
    return m_showDelay;
}

void InhibitWarnView::setAcceptReason(const QString &reason)
{
    m_acceptBtn->setText(reason);
}

void InhibitWarnView::setAcceptVisible(const bool acceptable)
{
    m_acceptBtn->setVisible(acceptable);
}

bool InhibitWarnView::hasInhibit() const
{
    return !m_inhibitorPtrList.isEmpty();
}

QString InhibitWarnView::iconString()
{
    QString icon_string;
    switch (m_inhibitType) {
    case SessionBaseModel::PowerAction::RequireShutdown:
    case SessionBaseModel::PowerAction::RequireUpdateShutdown:
        icon_string = ":/img/inhibitview/poweroff_warning.svg";
        break;
    case SessionBaseModel::PowerAction::RequireLogout:
        icon_string = ":/img/inhibitview/logout_warning.svg";
        break;
    default:
        icon_string = ":/img/inhibitview/reboot_warning.svg";
        break;
    }
    return icon_string;
}

bool InhibitWarnView::focusNextPrevChild(bool next)
{
    if (!next) {
        qCWarning(DDE_SHELL) << "Focus handling error, next prevent child is false";
        return WarningView::focusNextPrevChild(next);
    }

    return WarningView::focusNextPrevChild(next);
}

void InhibitWarnView::keyPressEvent(QKeyEvent *event)
{
    // 如果DSpinner正在播放，此时用户按下的esc键，DSpinner将会停止播放，因此在这里直接返回
    if (m_loading->isPlaying() && event->key() == Qt::Key_Escape)
        return;

    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down || event->key() == Qt::Key_Tab) {
        QWidget *currentFocusWidget = focusWidget();
        QWidget *nextFocusWidget = nullptr;

        if (currentFocusWidget == m_cancelBtn) {
            nextFocusWidget = m_acceptBtn;
        } else if (currentFocusWidget == m_acceptBtn) {
            nextFocusWidget = m_cancelBtn;
        }

        if (nextFocusWidget) {
            nextFocusWidget->setFocus();
        } else {
            m_cancelBtn->setFocus();
        }
    }

    QWidget::keyPressEvent(event);
}

void InhibitWarnView::initUi()
{
    QIcon acceptIcon = QIcon::fromTheme(iconString());

    m_acceptBtn->setObjectName("AcceptButton");
    m_acceptBtn->setFixedSize(ButtonWidth, ButtonHeight);
    m_acceptBtn->setFocusPolicy(Qt::StrongFocus);
    m_acceptBtn->setNormalPixmap(acceptIcon.pixmap(iconSize * devicePixelRatioF()));
    m_acceptBtn->setHoverPixmap(acceptIcon.pixmap(iconSize * devicePixelRatioF()));

    QIcon iconCancelNormal = QIcon::fromTheme(":/img/inhibitview/cancel_normal.svg");
    QIcon iconCancelHover = QIcon::fromTheme(":/img/inhibitview/cancel_hover.svg");

    m_cancelBtn->setNormalPixmap(iconCancelNormal.pixmap(iconSize * devicePixelRatioF()));
    m_cancelBtn->setHoverPixmap(iconCancelHover.pixmap(iconSize * devicePixelRatioF()));

    m_cancelBtn->setObjectName("CancelButton");
    m_cancelBtn->setFixedSize(ButtonWidth, ButtonHeight);
    m_cancelBtn->setText(tr("Cancel"));
    m_cancelBtn->setFocusPolicy(Qt::StrongFocus);

    auto inhibitorListWidget = new QWidget(this);
    inhibitorListWidget->setFixedWidth(FIXED_INHIBITOR_WIDTH);
    m_inhibitorListLayout = new QVBoxLayout(inhibitorListWidget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_inhibitorListLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_inhibitorListLayout->setMargin(0);
#endif
    m_inhibitorListLayout->setSpacing(10);
    QVBoxLayout *buttonLayout = new QVBoxLayout(m_bottomWidget);
    buttonLayout->setAlignment(Qt::AlignBottom);

    QWidget *textWidget = new QWidget(m_bottomWidget);
    QHBoxLayout *textLayout = new QHBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(10);
    m_loading = new Dtk::Widget::DSpinner(textWidget);
    m_confirmTextLabel = new QLabel(textWidget);
    m_confirmTextLabel->setText(tr("The reason of inhibit."));
    m_confirmTextLabel->setAlignment(Qt::AlignCenter);

    DFontSizeManager::instance()->bind(m_confirmTextLabel, DFontSizeManager::T5);
    textLayout->addStretch();
    textLayout->addWidget(m_loading);
    textLayout->addWidget(m_confirmTextLabel);
    textLayout->addStretch();

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addStretch();

    centralLayout->addWidget(inhibitorListWidget, 0, Qt::AlignHCenter);
    centralLayout->addSpacing(50);
    centralLayout->addWidget(m_bottomWidget);
    centralLayout->addStretch();
    setLayout(centralLayout);

    buttonLayout->addWidget(textWidget);
    buttonLayout->addSpacing(70);
    buttonLayout->addWidget(m_cancelBtn, 0, Qt::AlignHCenter);
    buttonLayout->addSpacing(15);
    buttonLayout->addWidget(m_acceptBtn, 0, Qt::AlignHCenter);

    setFocusPolicy(Qt::ClickFocus);
    m_loading->hide();
}

void InhibitWarnView::initConnection()
{
    connect(m_cancelBtn, &InhibitButton::clicked, this, &InhibitWarnView::cancelled);
    connect(m_acceptBtn, &InhibitButton::clicked, this, &InhibitWarnView::onAccept);
}

void InhibitWarnView::onAccept()
{
    if (!m_inhibitorPtrList.isEmpty()) {
        int height = m_bottomWidget->height();
        m_cancelBtn->hide();
        m_acceptBtn->hide();
        m_bottomWidget->setFixedHeight(height);
        static_cast<QVBoxLayout *>(m_bottomWidget->layout())->addStretch();
    }

    emit actionInvoked();
}
