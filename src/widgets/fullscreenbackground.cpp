// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fullscreenbackground.h"

#include "black_widget.h"
#include "public_func.h"
#include "sessionbasemodel.h"

#include <DGuiApplicationHelper>

#include <QDebug>
#include <QImageReader>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

Q_LOGGING_CATEGORY(DDE_SS, "dss.active")

DGUI_USE_NAMESPACE

const int PIXMAP_TYPE_BACKGROUND = 0;
const int PIXMAP_TYPE_BLUR_BACKGROUND = 1;

QString FullscreenBackground::backgroundPath;
QString FullscreenBackground::blurBackgroundPath;

QList<QPair<QSize, QPixmap>> FullscreenBackground::backgroundCacheList;
QList<QPair<QSize, QPixmap>> FullscreenBackground::blurBackgroundCacheList;
QList<FullscreenBackground *> FullscreenBackground::frameList;

FullscreenBackground::FullscreenBackground(SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_fadeOutAni(nullptr)
    , m_imageEffectInter(new ImageEffectInter("com.deepin.daemon.ImageEffect", "/com/deepin/daemon/ImageEffect", QDBusConnection::systemBus(), this))
    , m_model(model)
    , m_useSolidBackground(false)
    , m_fadeOutAniFinished(false)
    , m_enableAnimation(true)
    , m_blackWidget(new BlackWidget(this))
{
#ifndef QT_DEBUG
    if (!m_model->isUseWayland()) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    } else {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::Window);

        setAttribute(Qt::WA_NativeWindow); // 创建窗口 handle
        // onScreenDisplay 低于override，高于tooltip，希望显示在锁屏上方的界面，均需要调整层级为onScreenDisplay或者override
        windowHandle()->setProperty("_d_dwayland_window-type", "onScreenDisplay");
    }
#endif
    frameList.append(this);
    m_useSolidBackground = getDConfigValue(getDefaultConfigFileName(), "useSolidBackground", false).toBool();
    m_enableAnimation = DGuiApplicationHelper::isSpecialEffectsEnvironment();
    if (m_enableAnimation && !m_useSolidBackground) {
        m_fadeOutAni = new QVariantAnimation(this);
        m_fadeOutAni->setEasingCurve(QEasingCurve::InOutCubic);
        m_fadeOutAni->setDuration(2000);
        m_fadeOutAni->setStartValue(0.0);
        m_fadeOutAni->setEndValue(1.0);

        connect(m_fadeOutAni, &QVariantAnimation::valueChanged, this, [ this ] {
            update();
            // 动画播放完毕后不在需要清晰的壁纸，释放资源
            if (m_fadeOutAni->currentValue() == 1.0) {
                backgroundCacheList.clear();
                backgroundPath = "";
                m_fadeOutAniFinished = true;
            }
        });
    }


    m_blackWidget->setBlackMode(m_model->isBlackMode());
    connect(m_model, &SessionBaseModel::blackModeChanged, m_blackWidget, &BlackWidget::setBlackMode);
}

FullscreenBackground::~FullscreenBackground()
{
    frameList.removeAll(this);
}

void FullscreenBackground::updateBackground(const QString &path)
{
    if (m_useSolidBackground)
        return;

    if (isPicture(path)) {
        // 动画播放完毕不再需要清晰的背景图片
        if (!m_fadeOutAniFinished && !(backgroundPath == path && contains(PIXMAP_TYPE_BACKGROUND))) {
            backgroundPath = path;

            QPixmap pixmap;
            loadPixmap(backgroundPath, pixmap);
            addPixmap(pixmapHandle(pixmap), PIXMAP_TYPE_BACKGROUND);
        }

        // 需要播放动画的时候才更新模糊壁纸
        if (m_enableAnimation)
            updateBlurBackground(path);
    }
}

void FullscreenBackground::updateBlurBackground(const QString &path)
{
    QDBusPendingCall async = m_imageEffectInter->Get("", path);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(async, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] (QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        if (!reply.isError()) {
            QString blurPath = reply.value();

            if (!isPicture(blurPath)) {
                blurPath = "/usr/share/backgrounds/default_background.jpg";
            }

            if (blurBackgroundPath != blurPath || !contains(PIXMAP_TYPE_BLUR_BACKGROUND)) {
                blurBackgroundPath = blurPath;

                QPixmap pixmap;
                loadPixmap(blurBackgroundPath, pixmap);
                addPixmap(pixmapHandle(pixmap), PIXMAP_TYPE_BLUR_BACKGROUND);
            }

            // 只播放一次动画，后续背景图片变更直接更新模糊壁纸即可
            if (m_fadeOutAni && !m_fadeOutAniFinished)
                m_fadeOutAni->start();
            else
                update();
        } else {
            qWarning() << "get blur background image error: " << reply.error().message();
        }
        call->deleteLater();
    }, Qt::QueuedConnection);
}

bool FullscreenBackground::contentVisible() const
{
    return m_content && m_content->isVisible();
}

void FullscreenBackground::setEnterEnable(bool enable)
{
    m_enableEnterEvent = enable;
}

void FullscreenBackground::setScreen(QPointer<QScreen> screen, bool isVisible)
{
    if (screen.isNull())
        return;

    qInfo() << Q_FUNC_INFO
            << " screen info: " << screen
            << " screen geometry: " << screen->geometry()
            << " lock frame object:" << this;
    if (isVisible) {
        m_content->show();
        emit contentVisibleChanged(true);
    } else {
        // 如果有多个屏幕则根据鼠标所在的屏幕来显示
        QTimer::singleShot(1000, this, [this] {
            setMouseTracking(true);
        });
    }

    updateScreen(screen);
}

void FullscreenBackground::setContentVisible(bool visible)
{
    if (this->contentVisible() == visible)
        return;

    if (!m_content)
        return;

    if (!isVisible() && !visible)
        return;

    m_content->setVisible(visible);

    emit contentVisibleChanged(visible);
}

void FullscreenBackground::setContent(QWidget *const w)
{
    m_content = w;
    m_content->setParent(this);
    m_content->move(0, 0);
    m_content->setFocus();
    setFocusProxy(m_content);
    // 如果是黑屏状态则不置顶
    if (!m_blackWidget->isVisible())
        m_content->raise();
}

void FullscreenBackground::setIsHibernateMode()
{
    updateGeometry();
    m_content->show();
    emit contentVisibleChanged(true);
}

bool FullscreenBackground::isPicture(const QString &file)
{
    return QFile::exists(file) && QFile(file).size() && checkPictureCanRead(file);
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

    const QPixmap &background = getPixmap(PIXMAP_TYPE_BACKGROUND);
    const QPixmap &blurBackground = getPixmap(PIXMAP_TYPE_BLUR_BACKGROUND);

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));
    if (m_useSolidBackground) {
        painter.fillRect(trueRect, QColor(DDESESSIONCC::SOLID_BACKGROUND_COLOR));
    } else {
        if (m_fadeOutAni) {
            const double currentAniValue = m_fadeOutAni->currentValue().toDouble();
            if (!background.isNull() && currentAniValue != 1.0) {
                painter.setOpacity(1);
                painter.drawPixmap(trueRect,
                                background,
                                QRect(trueRect.topLeft(), trueRect.size() * background.devicePixelRatioF()));
            }

            if (!blurBackground.isNull()) {
                painter.setOpacity(currentAniValue);
                painter.drawPixmap(trueRect,
                                blurBackground,
                                QRect(trueRect.topLeft(), trueRect.size() * blurBackground.devicePixelRatioF()));
            }
        } else {
            // 不使用的动画的过渡的时候直接绘制清晰的背景
            if (!background.isNull()) {
                painter.drawPixmap(trueRect,
                                background,
                                QRect(trueRect.topLeft(), trueRect.size() * background.devicePixelRatioF()));
            }
        }
    }
}

void FullscreenBackground::tryActiveWindow(int count/* = 9*/)
{
    if (count < 0 || m_model->isUseWayland())
        return;

    qCDebug(DDE_SS) << "try active window..." << count;
    if (isActiveWindow()) {
        qCDebug(DDE_SS) << "...finally activewindow is me ...";
        return;
    }

    activateWindow();

    if (m_content && !m_content->isVisible()) {
        qCDebug(DDE_SS) << "hide..." << count;
        return;
    }
    QTimer::singleShot(50 , this, std::bind(&FullscreenBackground::tryActiveWindow, this, count -1));
}

void FullscreenBackground::enterEvent(QEvent *event)
{
    if (m_enableEnterEvent && m_model->visible()) {
        m_content->show();
        emit contentVisibleChanged(true);
        // 多屏情况下，此Frame晚于其它Frame显示出来时，可能处于未激活状态（特别是在wayland环境下比较明显）
        activateWindow();
    }

    // 锁屏截图之后 activewindow 不是锁屏了，此时发现不是 activewindow 主动尝试激活
    if (!isActiveWindow() && m_content && m_content->isVisible())
        tryActiveWindow();

    return QWidget::enterEvent(event);
}

void FullscreenBackground::leaveEvent(QEvent *event)
{
    return QWidget::leaveEvent(event);
}

void FullscreenBackground::resizeEvent(QResizeEvent *event)
{
    m_blackWidget->resize(size());
    m_content->resize(size());
    if (isPicture(backgroundPath) && !contains(PIXMAP_TYPE_BACKGROUND)) {
        QPixmap pixmap;
        loadPixmap(backgroundPath, pixmap);
        addPixmap(pixmapHandle(pixmap), PIXMAP_TYPE_BACKGROUND);
    }
    if (isPicture(blurBackgroundPath) && !contains(PIXMAP_TYPE_BLUR_BACKGROUND)) {
        QPixmap pixmap;
        loadPixmap(blurBackgroundPath, pixmap);
        addPixmap(pixmapHandle(QPixmap(blurBackgroundPath)), PIXMAP_TYPE_BLUR_BACKGROUND);
    }

    updatePixmap();
    QWidget::resizeEvent(event);
}

/**
 * @brief 鼠标移动触发这个事件，可能需要配合setMouseTracking(true)使用
 *
 * @param event
 */
void FullscreenBackground::mouseMoveEvent(QMouseEvent *event)
{
    if (m_model->visible() && m_enableEnterEvent) {
        m_content->show();
        emit contentVisibleChanged(true);
    }

    QWidget::mouseMoveEvent(event);
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
    if (m_model->isUseWayland()) {
        Q_EMIT requestDisableGlobalShortcutsForWayland(true);
    }

    updateGeometry();
    raise();
    return QWidget::showEvent(event);
}

void FullscreenBackground::hideEvent(QHideEvent *event)
{
    if (m_model->isUseWayland()) {
        Q_EMIT requestDisableGlobalShortcutsForWayland(false);
    }

    // 将content隐藏起来，下次锁屏拉起的时候显示鼠标所在屏幕，这样可以避免多屏显示认证界面（如果时序有问题的话）和 屏幕闪现认证界面的问题
    m_content->hide();
    QWidget::hideEvent(event);
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

void FullscreenBackground::updateScreen(QPointer<QScreen> screen)
{
    if (screen == m_screen)
        return;

    if (!m_screen.isNull())
        disconnect(m_screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry);

    if (!screen.isNull()) {
        connect(screen, &QScreen::geometryChanged, this, [=](){
            qInfo() << "screen geometry changed:" << screen << " lockframe:" << this;
        }, Qt::ConnectionType::QueuedConnection);
        connect(screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry, Qt::ConnectionType::QueuedConnection);
    }

    m_screen = screen;

    updateGeometry();
}

void FullscreenBackground::updateGeometry()
{
    qInfo() << "set background screen:" << m_screen;

    if (!m_screen.isNull()) {
        setGeometry(m_screen->geometry());
        qInfo() << "set background geometry:" << m_screen << m_screen->geometry() << "lockFrame:"
                << this  << " lockframe geometry:" << this->geometry();
    }
}

/********************************************************
 * 监听主窗体属性。
 * 用户登录界面，主窗体在某时刻会被设置为WindowDeactivate，
 * 此时登录界面获取不到焦点，需要调用requestActivate激活窗体。
********************************************************/
bool FullscreenBackground::event(QEvent *e)
{
#ifndef QT_DEBUG
    if (e->type() == QEvent::WindowDeactivate) {
        if (m_content->isVisible()) {
            tryActiveWindow();
        }
    }
#endif
    return QWidget::event(e);
}

const QPixmap& FullscreenBackground::getPixmap(int type)
{
    static QPixmap pixmap;

    const QSize &size = trueSize();
    auto findPixmap = [size](QList<QPair<QSize, QPixmap>> &list) -> const QPixmap & {
        auto it = std::find_if(list.begin(), list.end(),
                               [size](QPair<QSize, QPixmap> pair) { return pair.first == size; });
        return it != list.end() ? it.i->t().second : pixmap;
    };

    if (PIXMAP_TYPE_BACKGROUND == type)
        return findPixmap(backgroundCacheList);
    else
        return findPixmap(blurBackgroundCacheList);
}

QSize FullscreenBackground::trueSize() const
{
    return size() * devicePixelRatioF();
}

/**
 * @brief FullscreenBackground::addPixmap
 * 新增pixmap，存在相同size的则替换，没有则新增
 * @param pixmap pixmap对象
 * @param type 清晰壁纸还是模糊壁纸
 */
void FullscreenBackground::addPixmap(const QPixmap &pixmap, const int type)
{
    const QSize &size = trueSize();
    auto addFunc = [ this, &pixmap, size ] (QList<QPair<QSize, QPixmap>> &list){
        bool exist = false;
        for (auto &pair : list) {
            if (pair.first == size) {
                pair.second = pixmap;
                exist = true;
            }
        }
        if (!exist)
            list.append(QPair<QSize, QPixmap>(trueSize(), pixmap));
    };

    if (PIXMAP_TYPE_BACKGROUND == type)
        addFunc(backgroundCacheList);
    else
        addFunc(blurBackgroundCacheList);
}

/**
 * @brief FullscreenBackground::updatePixmap
 * 更新壁纸列表，移除当前所有屏幕都不会使用的壁纸数据
 */
void FullscreenBackground::updatePixmap()
{
    auto isNoUsefunc = [](const QSize &size) -> bool {
        bool b = std::any_of(frameList.begin(), frameList.end(), [size](FullscreenBackground *frame) {
            return frame->trueSize() == size;
        });
        return !b;
    };

    auto updateFunc = [ isNoUsefunc ] (QList<QPair<QSize, QPixmap>> &list) {
        for (auto &pair : list) {
            if (isNoUsefunc(pair.first))
                list.removeAll(pair);
        }
    };

    updateFunc(backgroundCacheList);
    updateFunc(blurBackgroundCacheList);
}

bool FullscreenBackground::contains(int type)
{
    auto containsFunc = [this](QList<QPair<QSize, QPixmap>> &list) -> bool {
        bool b = std::any_of(list.begin(), list.end(), [this](QPair<QSize, QPixmap> &pair) {
            return pair.first == trueSize();
        });
        return b;
    };

    if (PIXMAP_TYPE_BACKGROUND == type)
        return containsFunc(backgroundCacheList);
    else
        return containsFunc(blurBackgroundCacheList);
}

void FullscreenBackground::moveEvent(QMoveEvent *event)
{
    qInfo() << "FullscreenBackground::moveEvent: " << ", old pos: " << event->oldPos() << ", pos: " << event->pos();
    QWidget::moveEvent(event);
}
