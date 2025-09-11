// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fullscreenbackground.h"

#include "black_widget.h"
#include "lockcontent.h"
#include "public_func.h"
#include "sessionbasemodel.h"
#include "dconfig_helper.h"

#include <DGuiApplicationHelper>
#include <DPlatformWindowHandle>

#include <QDebug>
#include <QImageReader>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include "dbusconstant.h"

Q_LOGGING_CATEGORY(DDE_SS, "dss.active")

DGUI_USE_NAMESPACE

const int PIXMAP_TYPE_BACKGROUND = 0;
const int PIXMAP_TYPE_BLUR_BACKGROUND = 1;

QString FullScreenBackground::originBackgroundPath;
QString FullScreenBackground::blurBackgroundPath;

QMap<QString, QPixmap> FullScreenBackground::blurBackgroundCacheMap;
QList<FullScreenBackground *> FullScreenBackground::frameList;
QPointer<FullScreenBackground> FullScreenBackground::currentFrame = nullptr;
QPointer<QWidget> FullScreenBackground::currentContent = nullptr;

FullScreenBackground::FullScreenBackground(SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_useSolidBackground(false)
    , m_blackWidget(new BlackWidget(this))
    , m_resetGeometryTimer(new QTimer(this))
    , m_shutdownBlackWidget(nullptr)
{
#ifndef QT_DEBUG
    if (!m_model->isUseWayland()) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    } else {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::Window);

        setAttribute(Qt::WA_NativeWindow); // 创建窗口 handle
        // onScreenDisplay 低于override，高于tooltip，希望显示在锁屏上方的界面，均需要调整层级为onScreenDisplay或者override
        windowHandle()->setProperty("_d_dwayland_window-type", "onScreenDisplay");
        DPlatformWindowHandle handle(this, this);
        handle.setWindowRadius(-1);
    }
#endif
    frameList.append(this);
    m_useSolidBackground = DConfigHelper::instance()->getConfig("useSolidBackground", false).toBool();

    m_blackWidget->setBlackMode(m_model->isBlackMode());
    connect(m_model, &SessionBaseModel::blackModeChanged, this, [this](bool is_black) {
        m_blackWidget->setBlackMode(is_black);
        if (currentContent) {
            // 黑屏模式的时候需要隐藏中间的窗口
            currentContent->setVisible(!is_black);
            currentContent->raise();
        }
    });
    connect(m_resetGeometryTimer, &QTimer::timeout, this, [this] {
        const auto &currentGeometry = geometry();
        if (currentGeometry != m_geometryRect) {
            qCDebug(DDE_SHELL) << "Current geometry:" << currentGeometry <<"setGeometry:" << m_geometryRect;
            setGeometry(m_geometryRect);
        }
    });

    connect(m_model, &SessionBaseModel::shutdownkModeChanged, this, [this] (bool value){
        if (!m_shutdownBlackWidget) {
            m_shutdownBlackWidget = new ShutdownBlackWidget(this);
        }
        qCInfo(DDE_SHELL) << "FullScreenBackground size : " << size();
        m_shutdownBlackWidget->setFixedSize(this->size());
        m_shutdownBlackWidget->setBlackMode(value);
    });
}

FullScreenBackground::~FullScreenBackground()
{
    frameList.removeAll(this);
}

void FullScreenBackground::updateBackground(const QString &path)
{
    if (m_useSolidBackground || !isPicture(path))
        return;

    if (path == originBackgroundPath && !blurBackgroundPath.isEmpty()) {
        updateScreenBluBackground(blurBackgroundPath);
        return;
    }

    originBackgroundPath = path;
    updateBlurBackground(path);
}

void FullScreenBackground::updateScreenBluBackground(const QString &blurPath)
{
    if (!blurPath.isEmpty() && blurBackgroundPath != blurPath) {
        blurBackgroundPath = blurPath;
    }

    QString scaledPath;
    if (getScaledBlurImage(blurPath, scaledPath)) {
        QPixmap pixmap;
        loadPixmap(scaledPath, pixmap);
        addPixmap(pixmap, PIXMAP_TYPE_BLUR_BACKGROUND);
    } else {
        handleBackground(blurBackgroundPath, PIXMAP_TYPE_BLUR_BACKGROUND);
    }

    if (isVisible()) {
        update();
    }
}

void FullScreenBackground::updateBlurBackground(const QString &path)
{
    QDBusMessage message = QDBusMessage::createMethodCall(DSS_DBUS::imageEffectService, DSS_DBUS::imageEffectPath,
                                                          DSS_DBUS::imageEffectService, "Get");
    message << "" << path;
    QDBus::CallMode callMode = isVisible() ? QDBus::BlockWithGui : QDBus::Block;
    QDBusPendingReply<QString> reply = QDBusConnection::systemBus().call(message, callMode, 2 * 1000);
    QString blurPath;
    if (!reply.isError()) {
        blurPath = reply.value();
        bool isPicture = QFile::exists(blurPath) && QFile(blurPath).size() && checkPictureCanRead(blurPath);
        if (!isPicture) {
            blurPath = "/usr/share/backgrounds/default_background.jpg";
        }
    } else {
        blurPath = "/usr/share/backgrounds/default_background.jpg";
        qCWarning(DDE_SHELL) << "Get blur background path error:" << reply.error().message();
    }

    // 处理模糊背景图和执行动画或update；
    updateScreenBluBackground(blurPath);
}

bool FullScreenBackground::contentVisible() const
{
    return currentContent && currentContent->isVisible() && currentContent->parent() == this;
}

void FullScreenBackground::setEnterEnable(bool enable)
{
    m_enableEnterEvent = enable;
}

void FullScreenBackground::setScreen(QPointer<QScreen> screen, bool isVisible)
{
    if (screen.isNull()) {
        qCWarning(DDE_SHELL) << "Screen is nullptr";
        return;
    }

    qCInfo(DDE_SHELL) << "Set screen:" << screen
                      << ", screen geometry:" << screen->geometry()
                      << ", full screen background object:" << this
                      << ", visible:" << isVisible;
    if (isVisible) {
        updateCurrentFrame(this);
    } else {
        // 如果有多个屏幕则根据鼠标所在的屏幕来显示
        QTimer::singleShot(1000, this, [this] {
            setMouseTracking(true);
        });
    }

    updateScreen(screen);

    // 更新屏幕后设置壁纸，这样避免处理多个尺寸的壁纸
    updateBackground(m_model->currentUser()->greeterBackground());
}

void FullScreenBackground::setContent(QWidget *const w)
{
    if (!w) {
        qCWarning(DDE_SHELL) << "Content is null";
        return;
    }
    qCInfo(DDE_SHELL) << "Incoming  content:" << w << ", current content:" << currentContent;
    // 不重复设置content
    if (currentContent && currentContent->isVisible() && currentContent == w && currentFrame && w->parent() == currentFrame) {
        qCDebug(DDE_SHELL) << "Parent is current frame";
        return;
    }

    if (!currentContent.isNull()) {
        currentContent->setParent(nullptr);
        currentContent->hide();
    }
    currentContent = w;
    if (!currentFrame) {
        qCWarning(DDE_SHELL) << "Current frame is null";
        return;
    }

    currentContent->setParent(currentFrame);
    currentContent->move(0, 0);
    currentContent->resize(currentFrame->size());
    currentFrame->setFocusProxy(currentContent);
    currentFrame->setFocus();
    currentFrame->activateWindow();
    // 如果是黑屏状态则不置顶
    if (!currentFrame->m_blackWidget->isVisible()) {
        currentContent->raise();
        currentContent->show();
    };
    auto lockContent = dynamic_cast<LockContent *>(w);
    if (lockContent)
        Q_EMIT lockContent->parentChanged();
}

void FullScreenBackground::setIsHibernateMode()
{
    updateGeometry();
    updateCurrentFrame(this);
}

bool FullScreenBackground::isPicture(const QString &file)
{
    return QFile::exists(file) && QFile(file).size() && checkPictureCanRead(file);
}

QString FullScreenBackground::getLocalFile(const QString &file)
{
    const QUrl url(file);
    return url.isLocalFile() ? url.toLocalFile() : url.url();
}

void FullScreenBackground::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));
    if (m_useSolidBackground) {
        painter.fillRect(trueRect, QColor(DDESESSIONCC::SOLID_BACKGROUND_COLOR));
    } else {
        const QPixmap &blurBackground = getPixmap(PIXMAP_TYPE_BLUR_BACKGROUND);
        if (blurBackground.isNull()) {
            painter.fillRect(trueRect, QColor(DDESESSIONCC::SOLID_BACKGROUND_COLOR));
        } else {
            painter.drawPixmap(trueRect,
                               blurBackground,
                               QRect(trueRect.topLeft(), trueRect.size() * devicePixelRatioF()));
        }
    }
}

void FullScreenBackground::tryActiveWindow(int count /* = 9*/)
{
    if (count < 0 || m_model->isUseWayland())
        return;

    qCDebug(DDE_SS) << "try active window..." << count;
    if (isActiveWindow()) {
        qCDebug(DDE_SS) << "...finally activewindow is me ...";
        return;
    }

    activateWindow();

    if (currentContent && !currentContent->isVisible()) {
        qCDebug(DDE_SS) << "hide..." << count;
        return;
    }
    QTimer::singleShot(50, this, std::bind(&FullScreenBackground::tryActiveWindow, this, count - 1));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void FullScreenBackground::enterEvent(QEnterEvent *event)
#else
void FullScreenBackground::enterEvent(QEvent *event)
#endif
{
    qCInfo(DDE_SS) << "Enter event, enable enter event:" << m_enableEnterEvent << ", visible:" << m_model->visible();
    if (m_enableEnterEvent && m_model->visible()) {
        updateCurrentFrame(this);
        // 多屏情况下，此Frame晚于其它Frame显示出来时，可能处于未激活状态（特别是在wayland环境下比较明显）
        activateWindow();
    }

    // 锁屏截图之后 active window 不是锁屏了，此时发现不是 active window 主动尝试激活
    if (!isActiveWindow() && contentVisible())
        tryActiveWindow();

    return QWidget::enterEvent(event);
}

void FullScreenBackground::leaveEvent(QEvent *event)
{
    return QWidget::leaveEvent(event);
}

void FullScreenBackground::resizeEvent(QResizeEvent *event)
{
    if (!blurBackgroundCacheMap.isEmpty() && isPicture(blurBackgroundPath) && !contains(PIXMAP_TYPE_BLUR_BACKGROUND)) {
        QString scaledPath;
        if (getScaledBlurImage(blurBackgroundPath, scaledPath)) {
            QPixmap pixmap;
            loadPixmap(scaledPath, pixmap);
            addPixmap(pixmap, PIXMAP_TYPE_BLUR_BACKGROUND);
        } else {
            handleBackground(blurBackgroundPath, PIXMAP_TYPE_BLUR_BACKGROUND);
        }
    }

    updatePixmap();

    m_blackWidget->resize(size());
    if (currentFrame == this && currentContent) {
        currentContent->resize(size());
    }

    QWidget::resizeEvent(event);
}

/**
 * @brief 鼠标移动触发这个事件，可能需要配合setMouseTracking(true)使用
 *
 * @param event
 */
void FullScreenBackground::mouseMoveEvent(QMouseEvent *event)
{
    if (m_model->visible() && m_enableEnterEvent) {
        updateCurrentFrame(this);
    }

    QWidget::mouseMoveEvent(event);
}

void FullScreenBackground::keyPressEvent(QKeyEvent *e)
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

void FullScreenBackground::showEvent(QShowEvent *event)
{
    qCDebug(DDE_SHELL) << "Frame is already displayed:" << this;
    if (m_model->isUseWayland()) {
        Q_EMIT requestDisableGlobalShortcutsForWayland(true);
    }
    // fix bug 155019 此处是针对wayland下屏保退出，因qt没有发送enterEvent事件，进而导致密码框没有显示的规避方案
    // x下面先激活其它窗口右键菜单，锁屏后，再激活dock右键菜单自动锁屏后，同样存在没有发送enterEvent事件的问题
    // 如果鼠标的位置在当前屏幕内且锁屏状态为显示，将密码框显示出来
    if (m_model->visible() && geometry().contains(QCursor::pos())) {
        currentContent->show();
        // 多屏情况下，此Frame晚于其它Frame显示出来时，可能处于未激活状态（特别是在wayland环境下比较明显）
        activateWindow();
    }

#ifndef ENABLE_DSS_SNIPE
    // setScreen中有设置updateGeometry，单屏不需要再次设置
    if (qApp->screens().size() > 1) {
        updateGeometry();
    }
#else
    updateGeometry();
#endif

    // 显示的时候需要置顶，截图在上方的话无法显示锁屏 见Bug-140545
    raise();

    m_blackWidget->setBlackMode(m_model->isBlackMode());

    return QWidget::showEvent(event);
}

void FullScreenBackground::hideEvent(QHideEvent *event)
{
    qCDebug(DDE_SHELL) << "Frame is hidden:" << this;
    if (m_model->isUseWayland()) {
        Q_EMIT requestDisableGlobalShortcutsForWayland(false);
    }

    QWidget::hideEvent(event);
}

void FullScreenBackground::updateScreen(QPointer<QScreen> screen)
{
    if (screen == m_screen)
        return;

    if (!m_screen.isNull())
        disconnect(m_screen, &QScreen::geometryChanged, this, &FullScreenBackground::updateGeometry);

    if (!screen.isNull()) {
        connect(screen, &QScreen::geometryChanged, this, &FullScreenBackground::updateGeometry);
    }

    m_screen = screen;

    updateGeometry();
}

void FullScreenBackground::updateGeometry()
{
    if (m_screen.isNull()) {
        return;
    }

    qCInfo(DDE_SHELL) << "Set background screen:" << m_screen << ", geometry:" << m_screen->geometry();

    // for bug:184943.系统修改分辨率后，登录界面获取的屏幕分辨率不正确,通过xrandr获取屏幕分辨率
    if (m_model->appType() == AuthCommon::Login && !m_model->isUseWayland()) {
        const auto screensGeometry = getScreenGeometryByXrandr();

        if (screensGeometry.contains(m_screen->name())) {
            // 如果qt获取的屏幕分辨率和xrandr获取的屏幕分辨率一致，使用qt获取的屏幕geometry
            QSize qtScreenSize = m_screen->size();
            QSize xrandrScreenSize = screensGeometry[m_screen->name()].size();
            if (qtScreenSize == xrandrScreenSize) {
                // fix205519。获取到的屏幕尺寸一致，但qt的位置可能是错的，但不影响窗口位置。
                setddeGeometry(m_screen->geometry());
            } else {
                setddeGeometry(screensGeometry[m_screen->name()]);
                qCDebug(DDE_SHELL) << "Set geometry by xrandr, this:" << this << screensGeometry[m_screen->name()]
                                   << " screen:" << m_screen->name() << "screen geometry:" << m_screen->geometry();
            }
        } else {
            setddeGeometry(m_screen->geometry());
        }

        return;
    }

    if (!m_screen.isNull()) {
        if(m_model->isUseWayland())
            windowHandle()->setScreen(m_screen);
        setddeGeometry(m_screen->geometry());

        qCInfo(DDE_SHELL) << "Update geometry, screen:" << m_screen
                          << ", screen geometry:" << m_screen->geometry()
                          << ", lockFrame:" << this
                          << ", frame geometry:" << this->geometry();
    } else {
        qCWarning(DDE_SHELL) << "Screen is nullptr";
    }
}

/********************************************************
 * 监听主窗体属性。
 * 用户登录界面，主窗体在某时刻会被设置为WindowDeactivate，
 * 此时登录界面获取不到焦点，需要调用requestActivate激活窗体。
********************************************************/
bool FullScreenBackground::event(QEvent *e)
{
#ifndef QT_DEBUG
    if (e->type() == QEvent::WindowDeactivate) {
        if (contentVisible()) {
            tryActiveWindow();
        }
    }
#endif

    if (e->type() == QEvent::MouseButtonPress) {
        setFocus();
    }

    return QWidget::event(e);
}

const QPixmap &FullScreenBackground::getPixmap(int type)
{
    static QPixmap nullPixmap;

    QString strSize = sizeToString(trueSize());
    if (PIXMAP_TYPE_BLUR_BACKGROUND == type && blurBackgroundCacheMap.contains(strSize)) {
        return blurBackgroundCacheMap[strSize];
    }

    return nullPixmap;
}

QSize FullScreenBackground::trueSize() const
{
    return size() * devicePixelRatioF();
}

/**
 * @brief FullScreenBackground::addPixmap
 * 新增pixmap，存在相同size的则替换，没有则新增
 * @param pixmap pixmap对象
 * @param type 清晰壁纸还是模糊壁纸
 */
void FullScreenBackground::addPixmap(const QPixmap &pixmap, const int type)
{
    QString strSize = sizeToString(trueSize());
    if (type == PIXMAP_TYPE_BLUR_BACKGROUND) {
        blurBackgroundCacheMap[strSize] = pixmap;
    }
}

/**
 * @brief FullScreenBackground::updatePixmap
 * 更新壁纸列表，移除当前所有屏幕都不会使用的壁纸数据
 */
void FullScreenBackground::updatePixmap()
{
    auto removeNotUsedPixmap = [](QMap<QString, QPixmap> &cacheMap, const QStringList &frameSizeList) {
        QStringList cacheMapKeys = cacheMap.keys();
        for (auto &key : cacheMapKeys) {
            if (!frameSizeList.contains(key)) {
                qCInfo(DDE_SHELL) << "Remove not used pixmap:" << key;
                cacheMap.remove(key);
            }
        }
    };

    QStringList frameSizeList;
    for (auto &frame : frameList) {
        QString strSize = sizeToString(frame->trueSize());
        frameSizeList.append(strSize);
    }

    removeNotUsedPixmap(blurBackgroundCacheMap, frameSizeList);
}

bool FullScreenBackground::contains(int type)
{
    auto containPixmap = [](const QMap<QString, QPixmap> &cacheMap, const QString &strSize) {
        if (cacheMap.contains(strSize)) {
            const QPixmap &pixmap = cacheMap[strSize];
            return !pixmap.isNull(); // pixmap 为空也返回false
        } else {
            return false;
        }
    };

    QString strSize = sizeToString(trueSize());
    if (PIXMAP_TYPE_BLUR_BACKGROUND == type) {
        return containPixmap(blurBackgroundCacheMap, strSize);
    }
    return false;
}

void FullScreenBackground::moveEvent(QMoveEvent *event)
{
    // 规避 bug189309，登录时后端屏幕变化通知太晚
    if (currentContent && !currentContent->isVisible()) {
        if (m_model->isUseWayland()) {
            if (m_model->visible() && geometry().contains(QCursor::pos())) {
                currentContent->show();
            }
        }
    }
    QWidget::moveEvent(event);
}

QMap<QString, QRect> FullScreenBackground::getScreenGeometryByXrandr()
{
    QMap<QString, QRect> screensGeometry;

    // 获取缩放比例
    double scale = getScaleFactorFromDisplay();
    double scaleFromDisplayQt = qApp->devicePixelRatio();
    qCInfo(DDE_SHELL) << "Get scale factor from display scale:" << scale << " scale from display qt:" << scaleFromDisplayQt;
    // 如果获取失败或者与从Qt获取的不一致，则不需要处理，使用Qt的屏幕信息
    if (scale <= 0 || scaleFromDisplayQt != scale) {
        return screensGeometry;
    }

    // 启动 xrandr | grep connected 进程
    QProcess process;
    QStringList arguments;
    arguments << "-c"
              << "xrandr | grep connected";
    process.start("/bin/sh", arguments);
    process.waitForStarted();
    process.waitForFinished();

    // 读取进程输出并解析屏幕信息
    QString output = QString::fromLocal8Bit(process.readAll());
    QRegularExpression regex("(\\S+) connected (?:primary )?(\\d+)x(\\d+)\\+(\\d+)\\+(\\d+)");
    QRegularExpressionMatchIterator iter = regex.globalMatch(output);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        if (match.hasMatch()) {
            QString name = match.captured(1);
            int width = match.captured(2).toInt();
            int height = match.captured(3).toInt();
            int x = match.captured(4).toInt();
            int y = match.captured(5).toInt();
            QRect rect(x, y, width / scale, height / scale);
            screensGeometry.insert(name, rect);
            qCInfo(DDE_SHELL) << "Screen name:" << name << ", screen rect:" << rect;
        }
    }

    return screensGeometry;
}

double FullScreenBackground::getScaleFactorFromDisplay()
{
    QDBusInterface display(DSS_DBUS::systemDisplayService,
                           DSS_DBUS::systemDisplayPath,
                           DSS_DBUS::systemDisplayService,
                           QDBusConnection::systemBus(), this);

    QDBusReply<QString> reply = display.call("GetConfig");
    QString jsonObjStr = reply.value();
    if (jsonObjStr.isNull()) {
        qCWarning(DDE_SHELL) << "Greeter get system display config failed (`GetConfig` null)";
        return -1.0;
    }

    // 获取 ScaleFactors 对象
    QJsonObject json = QJsonDocument::fromJson(jsonObjStr.toUtf8()).object();
    QJsonObject scaleFactors = json.value("Config").toObject().value("ScaleFactors").toObject();

    // 遍历 ScaleFactors 对象
    for (const auto &key : scaleFactors.keys()) {
        return scaleFactors.value(key).toDouble();
    }

    qCWarning(DDE_SHELL) << "greeter get system display config failed(`scaleFactors` null)";
    return -1.0;
}

void FullScreenBackground::updateCurrentFrame(FullScreenBackground *frame)
{
    if (!frame) {
        qCWarning(DDE_SHELL) << "Update current frame failed, frame is null";
        return;
    }

    if (frame->m_screen)
        qCInfo(DDE_SHELL) << "Update current frame:" << frame << ", screen:" << frame->m_screen->name();
    else
        qWarning() << "Frame's screen is null, frame:" << frame;

    currentFrame = frame;
    setContent(currentContent);
}

void FullScreenBackground::handleBackground(const QString &path, int type)
{
    QSize trueSize = this->trueSize();

    QPixmap pixmap;
    loadPixmap(path, pixmap);

    pixmap = pixmap.scaled(trueSize, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
    pixmap = pixmap.copy(QRect((pixmap.width() - trueSize.width()) / 2,
                               (pixmap.height() - trueSize.height()) / 2,
                               trueSize.width(),
                               trueSize.height()));

    // draw pix to widget, so pix need set pixel ratio from qwidget devicepixelratioF
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    addPixmap(pixmap, type);
}

QString FullScreenBackground::sizeToString(const QSize &size)
{
    return QString("%1x%2").arg(size.width()).arg(size.height());
}

bool FullScreenBackground::getScaledBlurImage(const QString &originPath, QString &scaledPath)
{
    // 为了兼容没有安装壁纸服务环境;Qt5.15高版本可以使用activatableServiceNames()遍历然后可判断系统有没有安装服务
    const QString wallpaperServicePath = "/lib/systemd/system/dde-wallpaper-cache.service";
    if (!QFile::exists(wallpaperServicePath)) {
        qWarning() << "dde-wallpaper-cache service not existed";
        return false;
    }

    // 壁纸服务dde-wallpaper-cache
    QDBusInterface wallpaperCacheInterface("org.deepin.dde.WallpaperCache", "/org/deepin/dde/WallpaperCache",
                                           "org.deepin.dde.WallpaperCache", QDBusConnection::systemBus());

    QFile file(originPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file.";
        return false;
    }
    const int fd = file.handle();
    if (fd <= 0) {
        qWarning() << "originPath file fd error";
        return false;
    }

    QDBusUnixFileDescriptor dbusFd(fd);
    const QString &pathmd5 = QCryptographicHash::hash(originPath.toUtf8(), QCryptographicHash::Md5).toHex();
    QVariantList sizeArray;
    sizeArray << QVariant::fromValue(this->trueSize());
    QDBusReply<QStringList> pathList = wallpaperCacheInterface.call("GetProcessedImagePathByFd", QVariant::fromValue(dbusFd), pathmd5, sizeArray);
    if (pathList.value().isEmpty()) {
        qWarning() << "GetProcessedImagePathByFd interface return null";
        return false;
    }

    QString path = pathList.value().at(0);
    if (!path.isEmpty() && path != originPath) {
        scaledPath = path;
        // 图片处理完之后会添加_xxxx尺寸后缀，升级场景第一次登录只能获取到正在处理的原生图片，此时不能进行设置
        if (!path.contains("_")) {
            return false;
        }
        qDebug() << "get scaled path:" << path;
        return true;
    }

    return false;
}

//增加一个定时器，每隔50ms再设置一次Geometry，避免出现xorg初始化未完成的情况，导致界面显示不全
void FullScreenBackground::setddeGeometry(const QRect &rect)
{
    setGeometry(rect);
    m_geometryRect = rect;
    m_resetGeometryTimer->start(200);
    QTimer::singleShot(400 * 5, m_resetGeometryTimer, &QTimer::stop);
}
