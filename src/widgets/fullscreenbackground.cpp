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
#include "sessionbasemodel.h"

#include <DGuiApplicationHelper>

#include <QDebug>
#include <QImageReader>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>

DGUI_USE_NAMESPACE

FullscreenBackground::FullscreenBackground(SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_fadeOutAni(new QVariantAnimation(this))
    , m_imageEffectInter(new ImageEffectInter("com.deepin.daemon.ImageEffect", "/com/deepin/daemon/ImageEffect", QDBusConnection::systemBus(), this))
    , m_model(model)
    , m_originalCursor(cursor())
{
#ifndef QT_DEBUG
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
#endif
    m_fadeOutAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_fadeOutAni->setDuration(2000);
    m_fadeOutAni->setStartValue(0.0);
    m_fadeOutAni->setEndValue(1.0);

    installEventFilter(this);

    connect(m_fadeOutAni, &QVariantAnimation::valueChanged, this, static_cast<void (FullscreenBackground::*)()>(&FullscreenBackground::update));
}

FullscreenBackground::~FullscreenBackground()
{
}

void FullscreenBackground::updateBackground(const QString &path)
{
    qDebug() << "FullscreenBackground::updateBackground:" << path;
    if (isPicture(path)) {
        m_background = QPixmap(path);
        updateBlurBackground(path);
    } else {
        QSharedMemory memory(path);
        if (memory.attach()) {
            QPixmap image;
            image.loadFromData(static_cast<const unsigned char *>(memory.data()), static_cast<unsigned>(memory.size()));
            m_background = image;
            memory.detach();
        } else {
            QString filePath = "/usr/share/wallpapers/deepin/desktop.jpg";
            if (!isPicture(filePath)) {
                filePath = "/usr/share/backgrounds/default_background.jpg";
            }
            m_background = QPixmap(filePath);
            updateBlurBackground(path);
        }
    }
    m_backgroundCache = pixmapHandle(m_background);
}

void FullscreenBackground::updateBlurBackground(const QString &path)
{
    QDBusPendingCall call = m_imageEffectInter->Get("", path);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [=] {
        if (!call.isError()) {
            QDBusReply<QString> reply = call.reply();
            QString path = reply.value();
            if (!isPicture(path)) {
                path = "/usr/share/backgrounds/default_background.jpg";
            }
            m_blurBackground = QPixmap(path);
            m_blurBackgroundCache = pixmapHandle(m_blurBackground);
            m_fadeOutAni->start();
        } else {
            qWarning() << "get blur background image error: " << call.error().message();
        }
        watcher->deleteLater();
    });
}

bool FullscreenBackground::contentVisible() const
{
    return m_content && m_content->isVisible();
}

void FullscreenBackground::enableEnterEvent(bool enable)
{
    m_enableEnterEvent = enable;
}

void FullscreenBackground::setScreen(QScreen *screen, bool isVisible)
{
    QScreen *primary_screen = QGuiApplication::primaryScreen();
    if (primary_screen == screen && !m_isBlackMode && isVisible) {
        m_content->show();
        m_primaryShowFinished = true;
        emit contentVisibleChanged(true);
    } else {
        QTimer::singleShot(1000, this, [=] {
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

    m_content->setVisible(contentVisible && !m_isBlackMode);

    emit contentVisibleChanged(contentVisible && !m_isBlackMode);
}

void FullscreenBackground::setContent(QWidget *const w)
{
    m_content = w;
    m_content->setParent(this);
    m_content->raise();
    m_content->move(0, 0);
    m_content->setFocus();
    setFocusProxy(m_content);
}

void FullscreenBackground::setIsBlackMode(bool is_black)
{
    if (m_isBlackMode == is_black) return;

    if (is_black) {
        //黑屏的同时隐藏鼠标
        setCursor(Qt::BlankCursor);
    } else {
        //唤醒后显示鼠标
        setCursor(m_originalCursor);
    }

    //黑屏鼠标在多屏间移动时不显示界面
    m_enableEnterEvent = !is_black;
    m_isBlackMode = is_black;

    m_content->setVisible(!m_isBlackMode);
    emit contentVisibleChanged(!m_isBlackMode);

    update();
}

void FullscreenBackground::setIsHibernateMode()
{
    updateGeometry();
    m_content->show();
    emit contentVisibleChanged(true);
}

bool FullscreenBackground::isPicture(const QString &file)
{
    return QFile::exists(file) && QFile(file).size() && QImageReader(file).canRead();
}

QString FullscreenBackground::getLocalFile(const QString &file)
{
    const QUrl url(file);
    return url.isLocalFile() ? url.toLocalFile() : url.url();
}

void FullscreenBackground::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const double currentAniValue = m_fadeOutAni->currentValue().toDouble();

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));
    if (!m_isBlackMode) {
        if (!m_background.isNull()) {
            painter.setOpacity(1);
            painter.drawPixmap(trueRect,
                               m_backgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_backgroundCache.devicePixelRatioF()));
        }
        if (!m_blurBackground.isNull()) {
            painter.setOpacity(currentAniValue);
            painter.drawPixmap(trueRect,
                               m_blurBackgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_blurBackgroundCache.devicePixelRatioF()));
        }
    } else {
        painter.fillRect(trueRect, Qt::black);
    }
}

void FullscreenBackground::enterEvent(QEvent *event)
{
    if (m_primaryShowFinished && m_enableEnterEvent && m_model->visible()) {
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
    m_blurBackgroundCache = pixmapHandle(m_blurBackground);

    QWidget::resizeEvent(event);
}

/**
 * @brief 鼠标移动触发这个事件，可能需要配合setMouseTracking(true)使用
 *
 * @param event
 */
void FullscreenBackground::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_isBlackMode && m_model->visible()) {
        m_content->show();
        emit contentVisibleChanged(true);
    }

    return QWidget::mouseMoveEvent(event);
}

void FullscreenBackground::keyPressEvent(QKeyEvent *e)
{
    QWidget::keyPressEvent(e);

    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:
        qApp->quit();
        break;
#endif
    default:
        break;
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
    const QSize trueSize {size() * devicePixelRatioF()};
    QPixmap pix;
    if (!pixmap.isNull())
        pix = pixmap.scaled(trueSize, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);

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
