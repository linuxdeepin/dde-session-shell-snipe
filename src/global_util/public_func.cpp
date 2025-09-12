// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "public_func.h"

#include "constants.h"

#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTranslator>
#include <QJsonDocument>
#include <QApplication>
#include <QScreen>
#include <QSurfaceFormat>
#include <QNetworkProxyQuery>

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>

#ifndef ENABLE_DSS_SNIPE
#include <QGSettings>
#else
#include <QDBusInterface>
#endif

using namespace std;

DCORE_USE_NAMESPACE

Q_LOGGING_CATEGORY(DDE_SHELL, "org.deepin.dde.shell")

static int appType = APP_TYPE_LOCK;

//Load the System cursor --begin
static XcursorImages*
xcLoadImages(const char *image, int size)
{
    QSettings settings(DDESESSIONCC::DEFAULT_CURSOR_THEME, QSettings::IniFormat);
    //The default cursor theme path
    qDebug() << "Theme Path:" << DDESESSIONCC::DEFAULT_CURSOR_THEME;
    QString item = "Icon Theme";
    const char* defaultTheme = "";
    QVariant tmpCursorTheme = settings.value(item + "/Inherits");
    if (tmpCursorTheme.isNull()) {
        qDebug() << "Set a default one instead, if get the cursor theme failed!";
        defaultTheme = "Deepin";
    } else {
        defaultTheme = tmpCursorTheme.toString().toLocal8Bit().data();
    }

    qDebug() << "Get defaultTheme:" << tmpCursorTheme.isNull()
             << defaultTheme;
    return XcursorLibraryLoadImages(image, defaultTheme, size);
}

static unsigned long loadCursorHandle(Display *dpy, const char *name, int size)
{
    if (size == -1) {
        size = XcursorGetDefaultSize(dpy);
    }

    // Load the cursor images
    XcursorImages *images = nullptr;
    images = xcLoadImages(name, size);

    if (!images) {
        images = xcLoadImages(name, size);
        if (!images) {
            return 0;
        }
    }

    unsigned long handle = static_cast<unsigned long>(XcursorImagesLoadCursor(dpy,images));
    XcursorImagesDestroy(images);

    return handle;
}

int setRootWindowCursor() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        qCWarning(DDE_SHELL) << "Open display failed, display is empty";
        return -1;
    }

    const char *cursorPath = qApp->devicePixelRatio() > 1.7
            ? "/usr/share/icons/bloom/cursors/loginspinner@2x"
            : "/usr/share/icons/bloom/cursors/loginspinner";

    Cursor cursor = static_cast<Cursor>(XcursorFilenameLoadCursor(display, cursorPath));
    if (cursor == 0) {
        cursor = static_cast<Cursor>(loadCursorHandle(display, "watch", 24));
    }
    XDefineCursor(display, XDefaultRootWindow(display),cursor);

    // XFixesChangeCursorByName is the key to change the cursor
    // and the XFreeCursor and XCloseDisplay is also essential.

    XFixesChangeCursorByName(display, cursor, "watch");

    XFreeCursor(display, cursor);
    XCloseDisplay(display);

    return 0;
}

// 初次启动时，设置一下鼠标的默认位置
void setPointer()
{
    auto set_position = [ = ] (QPoint p) {
        Display *dpy;
        dpy = XOpenDisplay(nullptr);

        if (!dpy) return;

        Window root_window;
        root_window = XRootWindow(dpy, 0);
        XSelectInput(dpy, root_window, KeyReleaseMask);
        XWarpPointer(dpy, None, root_window, 0, 0, 0, 0, p.x(), p.y());
        XFlush(dpy);
    };

    if (!qApp->primaryScreen()) {
        QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, [ = ] {
            static bool first = true;
            if (first) {
                set_position(qApp->primaryScreen()->geometry().center());
                first = false;
            }
        });
    } else {
        set_position(qApp->primaryScreen()->geometry().center());
    }
}

QPixmap loadPixmap(const QString &file, const QSize& size)
{
    qreal ratio = 1.0;
    qreal devicePixel = qApp->devicePixelRatio();

    QPixmap pixmap;

    if (!qFuzzyCompare(ratio, devicePixel) || size.isValid()) {
        QImageReader reader;
        reader.setDecideFormatFromContent(true);
        reader.setFileName(qt_findAtNxFile(file, devicePixel, &ratio));
        if (reader.canRead()) {
            reader.setScaledSize((size.isNull() ? reader.size() : reader.size().scaled(size, Qt::KeepAspectRatio)) * (devicePixel / ratio));
            pixmap = QPixmap::fromImage(reader.read());
            pixmap.setDevicePixelRatio(devicePixel);
        }
    } else {
        pixmap.load(file);
    }

    return pixmap;
}

//此处为下面函数的函数重载，用来解决Debug模式使用void loadPixmap(const QString &fileName, QPixmap &pixmap)调试崩溃问题，原因暂时不明，保险起见使用这个函数
QPixmap loadPixmap(const QString &fileName)
{
    QPixmap pixmap;
    if (!pixmap.load(fileName)) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            pixmap.loadFromData(file.readAll());
        }
    }

    return pixmap;
}

void loadPixmap(const QString &fileName, QPixmap &pixmap)
{
    if (!pixmap.load(fileName)) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            pixmap.loadFromData(file.readAll());
        }
    }
}

bool checkPictureCanRead(const QString &fileName)
{
    QImageReader reader;
    reader.setDecideFormatFromContent(true);
    reader.setFileName(fileName);
    return reader.canRead();
}

/**
 * @brief 是否使用域管认证。
 *
 * @return true 使用域管认证
 * @return false 使用系统认证
 */
bool isDeepinAuth()
{
#ifndef ENABLE_DSS_SNIPE
    const char* controlId = "com.deepin.dde.auth.control";
    if (QGSettings::isSchemaInstalled(controlId)) {
        const char *controlPath = "/com/deepin/dde/auth/control/";
        QGSettings controlObj(controlId, controlPath);
        const QString &key = "useDeepinAuth";
        bool useDeepinAuth = controlObj.keys().contains(key) && controlObj.get(key).toBool();
    #ifdef QT_DEBUG
        qDebug() << "Use deepin auth: " << useDeepinAuth;
    #endif
        return useDeepinAuth;
    }
#else
    // TODO this gsetting config provided by 域管
#endif
    return true;
}

void setAppType(int type)
{
    appType = type;
}

QString getDefaultConfigFileName()
{
    return APP_TYPE_LOCK == appType ? DDESESSIONCC::LOCK_DCONFIG_SOURCE : DDESESSIONCC::LOGIN_DCONFIG_SOURCE;
}

/**
 * @brief 加载翻译文件
 *
 * @param locale
 */
void loadTranslation(const QString &locale)
{
    static QTranslator translator;
    static QString localTmp;
    if (localTmp == locale) {
        return;
    }
    localTmp = locale;
    qApp->removeTranslator(&translator);
    translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + locale.split(".").first());
    qApp->installTranslator(&translator);
}

/**
 * @brief findSymLinTarget 查找软连接最终链接到的文件，如果不是软连接，直接返回原始路径
 * @param symLink 文件路径
 * @return 最终链接到的文件路径
 */
QString findSymLinTarget(const QString &symLink)
{
    QString file = symLink;
    QString tmpFile = symLink;
    while(!tmpFile.isEmpty()) {
        file = tmpFile;
        tmpFile = QFile::symLinkTarget(tmpFile);
    }

    return file;
}

/**
 * @brief json对象转为json字符串
 *
 * @param QJsonObject对象
 */
QString toJson(const QJsonObject &jsonObj)
{
    QJsonDocument doc;
    doc.setObject(jsonObj);
    return doc.toJson();
}

bool checkVersion(const QString &target, const QString &base)
{
    if (target == base) {
        return true;
    }
    const QStringList baseVersion = base.split(".");
    const QStringList targetVersion = target.split(".");

    const int baseVersionSize = baseVersion.size();
    const int targetVersionSize = targetVersion.size();
    const int size = baseVersionSize < targetVersionSize ? baseVersionSize : targetVersionSize;

    for (int i = 0; i < size; i++) {
        if (targetVersion[i] == baseVersion[i]) continue;
        return targetVersion[i].toInt() > baseVersion[i].toInt() ? true : false;
    }
    return true;
}

void configWebEngine()
{
    // 华为机型只支持OpenGLES，导致使用到webengine的地方都不能用
    // 登录锁屏用到了二维码解锁，二维码又依赖了webengine来显示
    if (qgetenv("XDG_SESSION_TYPE").contains("wayland")) {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        QSurfaceFormat::setDefaultFormat(format);
    }

    // 禁用 qwebengine 调试功能
    qunsetenv("QTWEBENGINE_REMOTE_DEBUGGING");

#ifdef __sw_64__
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--no-sandbox");
#endif

    // Disable function: Qt::AA_ForceRasterWidgets, solve the display problem of domestic platform (loongson mips)
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-web-security");
    qputenv("DXCB_FAKE_PLATFORM_NAME_XCB", "true");

    //龙芯机器配置,使得DApplication能正确加载QTWEBENGINE
    qputenv("DTK_FORCE_RASTER_WIDGETS", "FALSE");
    qputenv("_d_disableDBusFileDialog", "true");
    setenv("PULSE_PROP_media.role", "video", 1);

    // 使用--single-process可加快启动速度，sw 架构必须使用，否则页面加载不出来
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--single-process");
    // FIXME 在设置了自动代理的情况下，使用--single-process会导致崩溃，暂时去掉自动代理环境变量
    const QString auto_proxy = qgetenv("auto_proxy");
    if (!auto_proxy.isEmpty()) {
        qputenv("auto_proxy", "");
    }
    QNetworkProxyQuery query(QUrl("https://login.uniontech.com"));
    QList<QNetworkProxy> proxyList =  QNetworkProxyFactory::systemProxyForQuery(query);
    if (proxyList.size() > 0) {
        QNetworkProxy::setApplicationProxy(proxyList[0]);
    }
}

#ifdef ENABLE_DSS_SNIPE
bool isSleepLock()
{
    QDBusInterface powerInterface("org.deepin.dde.Power1",
        "/org/deepin/dde/Power1", "org.deepin.dde.Power1", QDBusConnection::sessionBus());

    if (!powerInterface.isValid()) {
        qCritical() << powerInterface.lastError().message();
        return true; // default
    }

    QVariant value = powerInterface.property("SleepLock");
    if (!value.isValid()) {
        qCritical() << "read SleepLock property failed";
        return true; // default
    }

    return value.toBool();
}
#endif

QString qtifyName(const QString &name) {
    bool next_cap = false;
    QString result;
    
    for (auto it = name.cbegin(); it != name.cend(); ++it) {
        if (*it == '-') {
            next_cap = true;
        } else if (next_cap) {
            result.append(it->toUpper());
            next_cap = false;
        } else {
            result.append(*it);
        }
    }
    
    return result;
}