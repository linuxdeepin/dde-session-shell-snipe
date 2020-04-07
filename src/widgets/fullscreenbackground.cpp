/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fullscreenbackground.h"

#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QPainter>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <QKeyEvent>
#include <QCryptographicHash>
#include <QWindow>
#include <QDir>
#include <DGuiApplicationHelper>

#include "src/session-widgets/framedatabind.h"

DGUI_USE_NAMESPACE

FullscreenBackground::FullscreenBackground(QWidget *parent)
    : QWidget(parent)
    , m_fadeOutAni(new QVariantAnimation(this))
    , m_imageEffectInter(new ImageEffectInter("com.deepin.daemon.ImageEffect", "/com/deepin/daemon/ImageEffect", QDBusConnection::systemBus(), this))
    , m_displayInter(new DisplayInter("com.deepin.daemon.Display", "/com/deepin/daemon/Display", QDBusConnection::sessionBus(), this))
{
#ifndef QT_DEBUG
    if(DGuiApplicationHelper::isXWindowPlatform()) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    }
#endif

    m_fadeOutAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_fadeOutAni->setDuration(1000);
    m_fadeOutAni->setStartValue(1.0f);
    m_fadeOutAni->setEndValue(0.0f);

    installEventFilter(this);

    connect(m_fadeOutAni, &QVariantAnimation::valueChanged, this, static_cast<void (FullscreenBackground::*)()>(&FullscreenBackground::update));
    std::function<void (QVariant)> primary_show_finish = std::bind(&FullscreenBackground::onOtherPagePrimaryShowChanged, this, std::placeholders::_1);
    m_dataBindIndex = FrameDataBind::Instance()->registerFunction("PrimaryShowFinished", primary_show_finish);
}

FullscreenBackground::~FullscreenBackground()
{
    FrameDataBind::Instance()->unRegisterFunction("PrimaryShowFinished", m_dataBindIndex);
}

bool FullscreenBackground::contentVisible() const
{
    return m_content && m_content->isVisible();
}

void FullscreenBackground::updateBackground(const QPixmap &background)
{
    // show old background fade out
    m_fakeBackground = m_background;
    m_background = background;

    m_backgroundCache = pixmapHandle(m_background);
    m_fakeBackgroundCache = pixmapHandle(m_fakeBackground);

    m_fadeOutAni->start();
}

void FullscreenBackground::updateBackground(const QString &file)
{
    qDebug() << "input background: " << file;

    auto isPicture = [](const QString &filePath){
        return QFile::exists(filePath) && QFile(filePath).size() && !QPixmap(filePath).isNull() ;
    };

    const QUrl url(file);
    if (url.isLocalFile())
        return updateBackground(url.path());

    if (m_bgPath == file)
        return;

    if (isPicture(file)) {
        m_bgPath = file;

    } else {
        QDir dir("/usr/share/wallpapers/deepin");
        if (dir.exists()) {
            dir.setFilter(QDir::Files);
            QFileInfoList list = dir.entryInfoList();
            foreach (QFileInfo f, list) {
                if (f.baseName() == "desktop") {
                    m_bgPath = f.filePath();
                    break;
                }
            }
        }

        if (!QFile::exists(m_bgPath)) {
            m_bgPath = "/usr/share/backgrounds/default_background.jpg";
        }
    }

    QString imageEffect = m_imageEffectInter->Get("", m_bgPath);

    if (!isPicture(imageEffect)) imageEffect = m_bgPath;

    updateBackground(QPixmap(imageEffect));

    Q_ASSERT(QFileInfo(m_bgPath).isFile());
}

void FullscreenBackground::setScreen(QScreen *screen)
{
    if(m_displayInter->displayMode() == FullscreenBackground::ExpandMode) {
        if(m_displayInter->primary() == screen->name()) {
            m_content->show();
            emit contentVisibleChanged(true);
            QTimer::singleShot(100, this, []{FrameDataBind::Instance()->updateValue("PrimaryShowFinished", true); });
        }
    } else if(m_displayInter->displayMode() == FullscreenBackground::CopyMode) {
        m_content->show();
        emit contentVisibleChanged(true);
    }

    updateScreen(screen);
}

void FullscreenBackground::setContentVisible(bool contentVisible)
{
    if (this->contentVisible() == contentVisible)
        return;

    if (!m_content)
        return;

    if (!isVisible() && !contentVisible)
        return;

    m_content->setVisible(contentVisible);

    emit contentVisibleChanged(contentVisible);
}

void FullscreenBackground::setContent(QWidget *const w)
{
    Q_ASSERT(m_content.isNull());

    m_content = w;
    m_content->setParent(this);
    m_content->raise();
    m_content->move(0, 0);
}

void FullscreenBackground::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const float current_ani_value = m_fadeOutAni->currentValue().toFloat();

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));

    if (!m_background.isNull()) {
        // tr is need redraw rect, sourceRect need correct upper left corner
        painter.drawPixmap(trueRect,
                           m_backgroundCache,
                           QRect(trueRect.topLeft(), trueRect.size() * m_backgroundCache.devicePixelRatioF()));
    }

    if (!m_fakeBackground.isNull()) {
        // draw background
        painter.setOpacity(current_ani_value);
        painter.drawPixmap(trueRect,
                           m_fakeBackgroundCache,
                           QRect(trueRect.topLeft(), trueRect.size() * m_fakeBackgroundCache.devicePixelRatioF()));
        painter.setOpacity(1);
    }
}

void FullscreenBackground::enterEvent(QEvent *event)
{
    if(m_primaryShowFinished) {
        m_content->show();
        emit contentVisibleChanged(true);
    }

    return QWidget::enterEvent(event);
}

void FullscreenBackground::leaveEvent(QEvent *event)
{
    return QWidget::leaveEvent(event);
}

void FullscreenBackground::resizeEvent(QResizeEvent *event)
{
    m_content->resize(size());

    m_backgroundCache = pixmapHandle(m_background);
    m_fakeBackgroundCache = pixmapHandle(m_fakeBackground);

    return QWidget::resizeEvent(event);
}

void FullscreenBackground::keyPressEvent(QKeyEvent *e)
{
    QWidget::keyPressEvent(e);

    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:        qApp->quit();       break;
#endif
    default:;
    }
}

void FullscreenBackground::showEvent(QShowEvent *event)
{
    if (QWindow *w = windowHandle()) {
        if (m_screen) {
            if (w->screen() != m_screen) {
                w->setScreen(m_screen);
            }

            // 更新窗口位置和大小
            updateGeometry();
        } else {
            updateScreen(w->screen());
        }

        connect(w, &QWindow::screenChanged, this, &FullscreenBackground::updateScreen);
    }

    return QWidget::showEvent(event);
}

const QPixmap FullscreenBackground::pixmapHandle(const QPixmap &pixmap)
{
    const QSize trueSize { size() *devicePixelRatioF() };
    QPixmap pix = pixmap.scaled(trueSize,
                                Qt::KeepAspectRatioByExpanding,
                                Qt::SmoothTransformation);

    pix = pix.copy(QRect((pix.width() - trueSize.width()) / 2,
                         (pix.height() - trueSize.height()) / 2,
                         trueSize.width(),
                         trueSize.height()));

    // draw pix to widget, so pix need set pixel ratio from qwidget devicepixelratioF
    pix.setDevicePixelRatio(devicePixelRatioF());

    return pix;
}

void FullscreenBackground::onOtherPagePrimaryShowChanged(const QVariant &value)
{
    m_primaryShowFinished = value.toBool();
}

void FullscreenBackground::updateScreen(QScreen *screen)
{
    if (screen == m_screen)
        return;

    if (m_screen) {
        disconnect(m_screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry);
    }

    if (screen) {
        connect(screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry);
    }

    m_screen = screen;

    if (m_screen)
        updateGeometry();
}

void FullscreenBackground::updateGeometry()
{
    setGeometry(m_screen->geometry());
}

/********************************************************
 * 监听主窗体属性。
 * 用户登录界面，主窗体在某时刻会被设置为WindowDeactivate，
 * 此时登录界面获取不到焦点，需要调用requestActivate激活窗体。
********************************************************/
bool FullscreenBackground::eventFilter(QObject *watched, QEvent *e)
{
#ifndef QT_DEBUG
    if (e->type() == QEvent::WindowDeactivate) {
        if (m_content->isVisible())
            windowHandle()->requestActivate();
    }
#endif
    return QWidget::eventFilter(watched, e);
}
