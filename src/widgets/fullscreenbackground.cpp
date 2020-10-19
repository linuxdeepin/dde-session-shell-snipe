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
#include <unistd.h>
#include "src/session-widgets/framedatabind.h"

DGUI_USE_NAMESPACE
#define  DEFAULT_BACKGROUND "/usr/share/backgrounds/default_background.jpg"

FullscreenBackground::FullscreenBackground(QWidget *parent)
    : QWidget(parent)
    , m_pixmapLoaded(false)
    , m_fadeOutAni(new QVariantAnimation(this))
    , m_imageEffectInter(new ImageEffectInter("com.deepin.daemon.ImageEffect", "/com/deepin/daemon/ImageEffect", QDBusConnection::systemBus(), this))
{
#ifndef QT_DEBUG
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
#endif

    m_fadeOutAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_fadeOutAni->setDuration(1000);
    m_fadeOutAni->setStartValue(1.0);
    m_fadeOutAni->setEndValue(0.0);

    installEventFilter(this);

    connect(m_fadeOutAni, &QVariantAnimation::valueChanged, this, static_cast<void (FullscreenBackground::*)()>(&FullscreenBackground::update));
}

FullscreenBackground::~FullscreenBackground()
{
}

bool FullscreenBackground::contentVisible() const
{
    return m_content && m_content->isVisible();
}

void FullscreenBackground::updateBackground()
{
    // show old background fade out
    m_fadeOutAni->start();
}

void FullscreenBackground::updateBackground(const QString &file)
{
    m_fakePath = m_backgroundPath;
    m_backgroundPath = file;
    if (isVisible()) {
        loadPixmap();
    }

    updateBackground();
}

QString FullscreenBackground::getBlurBackground (const QString &file)
{
    auto isPicture = [] (const QString & filePath) {
        return QFile::exists (filePath) && QFile (filePath).size() && !QPixmap (filePath).isNull() ;
    };

    QString bg_path = file;
    if (!isPicture(bg_path)) {
        QDir dir ("/usr/share/wallpapers/deepin");
        if (dir.exists()) {
            dir.setFilter (QDir::Files);
            QFileInfoList list = dir.entryInfoList();
            foreach (QFileInfo f, list) {
                if (f.baseName() == "desktop") {
                    bg_path = f.filePath();
                    break;
                }
            }
        }

        if (!QFile::exists (bg_path)) {
            bg_path = DEFAULT_BACKGROUND;
        }
    }

    QString imageEffect = m_imageEffectInter->Get ("", bg_path);
    if (!isPicture (imageEffect)) {
        imageEffect = DEFAULT_BACKGROUND;
    }

    return imageEffect;
}


void FullscreenBackground::setScreen(QScreen *screen)
{
    QScreen *primary_screen = QGuiApplication::primaryScreen();
    if(primary_screen == screen) {
        m_content->show();
        m_primaryShowFinished = true;
        emit contentVisibleChanged(true);
    } else {
        QTimer::singleShot(1000, this, [ = ] {
            m_primaryShowFinished = true;
            setMouseTracking(true);
        });
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
    //Q_ASSERT(m_content.isNull());

    m_content = w;
    m_content->setParent(this);
    m_content->raise();
    m_content->move(0, 0);
}

void FullscreenBackground::setIsBlackMode(bool is_black)
{
    if(m_isBlackMode == is_black) return;

    m_isBlackMode = is_black;
    FrameDataBind::Instance()->updateValue("PrimaryShowFinished", !is_black);
    m_content->setVisible(!is_black);
    emit contentVisibleChanged(!is_black);

    update();
}

void FullscreenBackground::setIsHibernateMode(){
    updateGeometry();
    m_content->show();
    emit contentVisibleChanged(true);
}

void FullscreenBackground::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const double currentAniValue = m_fadeOutAni->currentValue().toDouble();

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));

    if (!m_pixmapLoaded) {
        loadPixmap();
    }

    if (!m_isBlackMode) {
        if (!m_background.isNull()) {
            // tr is need redraw rect, sourceRect need correct upper left corner
            painter.drawPixmap(trueRect,
                               m_backgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_backgroundCache.devicePixelRatioF()));
        }

        if (!m_fakeBackground.isNull()) {
            // draw background
            painter.setOpacity(currentAniValue);
            painter.drawPixmap(trueRect,
                               m_fakeBackgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_fakeBackgroundCache.devicePixelRatioF()));
            painter.setOpacity(1);
        }
    } else {
        painter.fillRect(trueRect, Qt::black);
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

    return QWidget::resizeEvent(event);
}

void FullscreenBackground::mouseMoveEvent(QMouseEvent *event)
{
    m_content->show();
    emit contentVisibleChanged(true);

    return QWidget::mouseMoveEvent(event);
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
    m_pixmapLoaded = true;
    loadPixmap();

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

void FullscreenBackground::hideEvent(QHideEvent *event)
{
    // 解锁后，释放大量的图片内存占用
    m_pixmapLoaded = false;
    releasePixmap();
    return QWidget::hideEvent(event);
}

const QPixmap FullscreenBackground::pixmapHandle(const QPixmap &pixmap)
{
    const QSize trueSize { size() *devicePixelRatioF() };
    QPixmap pix;
    if (!pixmap.isNull())
        pix = pixmap.scaled(trueSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    pix = pix.copy(QRect((pix.width() - trueSize.width()) / 2,
                         (pix.height() - trueSize.height()) / 2,
                         trueSize.width(),
                         trueSize.height()));

    // draw pix to widget, so pix need set pixel ratio from qwidget devicepixelratioF
    pix.setDevicePixelRatio(devicePixelRatioF());

    return pix;
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

const QPixmap FullscreenBackground::getPixmapByPath(const QString &path)
{
    QPixmap image;
    QSharedMemory memory(path);
    if (memory.attach()) {
        image.loadFromData (static_cast<const unsigned char *>(memory.data()), static_cast<unsigned>(memory.size()));
        if (image.isNull()) {
            qDebug() << "input background: " << path << " is invalid image file.";
            image.load(getBlurBackground(path));
        }
        memory.detach();
    } else {
        image.load(getBlurBackground(path));
    }
    return image;
}

void FullscreenBackground::loadPixmap()
{
    m_background = getPixmapByPath(m_backgroundPath);
    m_backgroundCache = pixmapHandle(m_background);
    if (!m_fakePath.isEmpty()) {
        m_fakeBackground = getPixmapByPath(m_fakePath);
        m_fakeBackgroundCache = pixmapHandle(m_fakeBackground);
    }
}

void FullscreenBackground::releasePixmap()
{
    m_background = QPixmap();
    m_backgroundCache = QPixmap();
    m_fakeBackground = QPixmap();
    m_fakeBackgroundCache = QPixmap();
}
