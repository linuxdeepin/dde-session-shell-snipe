/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
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

#include "accessibilitycheckerex.h"
#include "appeventfilter.h"
#include "constants.h"
#include "greeterworker.h"
#include "loginwindow.h"
#include "modules_loader.h"
#include "multiscreenmanager.h"
#include "propertygroup.h"
#include "sessionbasemodel.h"

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DLog>
#include <DPlatformTheme>

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <cstdlib>
#include <DConfig>

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

const bool IsWayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");

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

static int set_rootwindow_cursor() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        qDebug() << "Open display failed";
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
// Load system cursor --end

static double get_scale_ratio() {
    Display *display = XOpenDisplay(nullptr);
    double scaleRatio = 0.0;

    if (!display) {
        return scaleRatio;
    }

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));

    if (!resources) {
        resources = XRRGetScreenResources(display, DefaultRootWindow(display));
        qWarning() << "get XRRGetScreenResourcesCurrent failed, use XRRGetScreenResources.";
    }

    if (resources) {
        for (int i = 0; i < resources->noutput; i++) {
            XRROutputInfo* outputInfo = XRRGetOutputInfo(display, resources, resources->outputs[i]);
            if (outputInfo->crtc == 0 || outputInfo->mm_width == 0) continue;

            XRRCrtcInfo *crtInfo = XRRGetCrtcInfo(display, resources, outputInfo->crtc);
            if (crtInfo == nullptr) continue;

            scaleRatio = static_cast<double>(crtInfo->width) / static_cast<double>(outputInfo->mm_width) / (1366.0 / 310.0);

            if (scaleRatio > 1 + 2.0 / 3.0) {
                scaleRatio = 2;
            }
            else if (scaleRatio > 1 + 1.0 / 3.0) {
                scaleRatio = 1.5;
            }
            else {
                scaleRatio = 1;
            }
        }
    }
    else {
        qWarning() << "get scale radio failed, please check X11 Extension.";
    }

    return scaleRatio;
}

// 读取系统配置文件的缩放比，如果是null，默认返回1
double getScaleFormConfig()
{
    double defaultScaleFactors = 1.0;
    DTK_CORE_NAMESPACE::DConfig * dconfig = DConfig::create("org.deepin.dde.lightdm-deepin-greeter", "org.deepin.dde.lightdm-deepin-greeter", QString(), nullptr);
    //华为机型,从override配置中获取默认缩放比
    if (dconfig) {
        defaultScaleFactors = dconfig->value("defaultScaleFactors", 1.0).toDouble() ;
        if(defaultScaleFactors < 1.0){
            defaultScaleFactors = 1.0;
        }
    }

    delete dconfig;
    dconfig = nullptr;

    QDBusInterface configInter("com.deepin.system.Display",
                                                     "/com/deepin/system/Display",
                                                     "com.deepin.system.Display",
                                                    QDBusConnection::systemBus());
    if (!configInter.isValid()) {
        return defaultScaleFactors;
    }
    QDBusReply<QString> configReply = configInter.call("GetConfig");
    if (configReply.error().message().isEmpty()) {
        QString config = configReply.value();
        QJsonParseError jsonError;
        QJsonDocument jsonDoc(QJsonDocument::fromJson(config.toStdString().data(), &jsonError));
        if (jsonError.error == QJsonParseError::NoError) {
            QJsonObject rootObj = jsonDoc.object();
            QJsonObject Config = rootObj.value("Config").toObject();
            double scaleFactors = Config.value("ScaleFactors").toObject().value("ALL").toDouble();
            qDebug() << "scaleFactors :" << scaleFactors;
            return scaleFactors;
        } else {
            return defaultScaleFactors;
        }
    } else {
        qWarning() << configReply.error().message();
        return defaultScaleFactors;
    }
}

static void set_auto_QT_SCALE_FACTOR() {
    const double ratio = IsWayland ? getScaleFormConfig() : get_scale_ratio();
    if (ratio > 0.0) {
        setenv("QT_SCALE_FACTOR", QByteArray::number(ratio).constData(), 1);
    }

    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        setenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1", 1);
    }
}

// 初次启动时，设置一下鼠标的默认位置
void set_pointer()
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

int main(int argc, char* argv[])
{
    // 正确加载dxcb插件
    //for qt5platform-plugins load DPlatformIntegration or DPlatformIntegrationParent
    if (!QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin")){
        setenv("XDG_CURRENT_DESKTOP", "Deepin", 1);
    }

    DGuiApplicationHelper::setAttribute(DGuiApplicationHelper::UseInactiveColorGroup, false);
    // 设置缩放，文件存在的情况下，由后端去设置，否则前端自行设置
    if (!QFile::exists("/etc/lightdm/deepin/xsettingsd.conf") || IsWayland) {
        set_auto_QT_SCALE_FACTOR();
    }

    DApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    qApp->setOrganizationName("deepin");
    qApp->setApplicationName("org.deepin.dde.lightdm-deepin-greeter");

    set_pointer();

    //注册全局事件过滤器
    AppEventFilter appEventFilter;
    a.installEventFilter(&appEventFilter);
    setAppType(APP_TYPE_LOGIN);

    DPalette pa = DGuiApplicationHelper::instance()->standardPalette(DGuiApplicationHelper::LightType);
    pa.setColor(QPalette::Normal, DPalette::WindowText, QColor("#FFFFFF"));
    pa.setColor(QPalette::Normal, DPalette::Text, QColor("#FFFFFF"));
    pa.setColor(QPalette::Normal, DPalette::AlternateBase, QColor(0, 0, 0, 76));
    pa.setColor(QPalette::Normal, DPalette::Button, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Light, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Dark, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::ButtonText, QColor("#FFFFFF"));
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::WindowText, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Text, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::AlternateBase, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Button, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Light, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Dark, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::ButtonText, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::instance()->setApplicationPalette(pa);

    DLogManager::registerConsoleAppender();

    dss::module::ModulesLoader *modulesLoader = &dss::module::ModulesLoader::instance();
    modulesLoader->start(QThread::LowestPriority);

    const QString serviceName = "com.deepin.daemon.Accounts";
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    if (!interface->isServiceRegistered(serviceName)) {
        qWarning() << "accounts service is not registered wait...";

        QDBusServiceWatcher *serviceWatcher = new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus());
        QObject::connect(serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, [ = ] (const QString &service) {
            if (service == serviceName) {
                qCritical() << "ERROR, accounts service unregistered, what should I do?";
            }
        });

#ifdef ENABLE_WAITING_ACCOUNTS_SERVICE
        qDebug() << "waiting for deepin accounts service";
        QEventLoop eventLoop;
        QObject::connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, &eventLoop, &QEventLoop::quit);
#ifdef  QT_DEBUG
        QTimer::singleShot(10000, &eventLoop, &QEventLoop::quit);
#endif
        eventLoop.exec();
        qDebug() << "service registered!";
#endif
    }

    SessionBaseModel *model = new SessionBaseModel();
    model->setAppType(Login);
    GreeterWorker *worker = new GreeterWorker(model);
    QObject::connect(&appEventFilter, &AppEventFilter::userIsActive, worker, &GreeterWorker::restartResetSessionTimer);

    /* load translation files */
    loadTranslation(model->currentUser()->locale());

    // 设置系统登录成功的加载光标
    QObject::connect(model, &SessionBaseModel::authFinished, model, [ = ](bool is_success) {
        if (is_success)
            set_rootwindow_cursor();
    });

    // 保证多个屏幕的情况下，始终只有一个屏幕显示
    PropertyGroup *property_group = new PropertyGroup(worker);
    property_group->addProperty("contentVisible");

    auto createFrame = [&](QPointer<QScreen> screen, int count) -> QWidget * {
        LoginWindow *loginFrame = new LoginWindow(model);
        loginFrame->setScreen(screen, count <= 0);
        property_group->addObject(loginFrame);
        QObject::connect(loginFrame, &LoginWindow::requestSwitchToUser, worker, &GreeterWorker::switchToUser);
        QObject::connect(loginFrame, &LoginWindow::requestSetKeyboardLayout, worker, &GreeterWorker::setKeyboardLayout);
        QObject::connect(loginFrame, &LoginWindow::requestCheckAccount, worker, &GreeterWorker::checkAccount);
        QObject::connect(loginFrame, &LoginWindow::requestStartAuthentication, worker, &GreeterWorker::startAuthentication);
        QObject::connect(loginFrame, &LoginWindow::sendTokenToAuth, worker, &GreeterWorker::sendTokenToAuth);
        QObject::connect(loginFrame, &LoginWindow::requestEndAuthentication, worker, &GreeterWorker::endAuthentication);
        QObject::connect(loginFrame, &LoginWindow::authFinished, worker, &GreeterWorker::onAuthFinished);
        QObject::connect(worker, &GreeterWorker::requestUpdateBackground, loginFrame, &LoginWindow::updateBackground);
        if (!IsWayland) {
            loginFrame->show();
        } else {
            QObject::connect(worker, &GreeterWorker::showLoginWindow, loginFrame, &LoginWindow::setVisible);
        }
        return loginFrame;
    };

    MultiScreenManager multi_screen_manager;
    multi_screen_manager.register_for_mutil_screen(createFrame);
    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);

#if defined(DSS_CHECK_ACCESSIBILITY) && defined(QT_DEBUG)
    AccessibilityCheckerEx checker;
    checker.addIgnoreClasses(QStringList()
                             << "Dtk::Widget::DBlurEffectWidget");
    checker.setOutputFormat(DAccessibilityChecker::FullFormat);
    checker.start();
#endif

    if (!IsWayland) {
        model->setVisible(true);
    }

    return a.exec();
}
