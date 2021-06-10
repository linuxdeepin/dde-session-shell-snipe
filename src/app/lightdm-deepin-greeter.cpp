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

#include "src/lightdm-deepin-greeter/loginwindow.h"
#include "src/global_util/constants.h"
#include "src/lightdm-deepin-greeter/greeterworkek.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/widgets/propertygroup.h"
#include "src/global_util/multiscreenmanager.h"
#include "src/lightdm-deepin-greeter/greeterscreenmanager.h"

#include <DApplication>
#include <QtCore/QTranslator>
#include <QLabel>
#include <QProcess>
#include <QThread>
#include <QSettings>
#include <QWindow>
#include <QScreen>
#include <DLog>
#include <QDesktopWidget>
#include <DGuiApplicationHelper>

#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib-xcb.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xfixes.h>

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

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

    unsigned long handle = (unsigned long)XcursorImagesLoadCursor(dpy,images);
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

    Cursor cursor = (Cursor)XcursorFilenameLoadCursor(display, cursorPath);
    if (cursor == 0) {
        cursor = (Cursor)loadCursorHandle(display, "watch", 24);
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

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));
    double scaleRatio = 0.0;

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

            scaleRatio = (double)crtInfo->width / (double)outputInfo->mm_width / (1366.0 / 310.0);

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

static void set_auto_QT_SCALE_FACTOR() {
    const double ratio = get_scale_ratio();
    if (ratio > 0.0) {
        setenv("QT_SCALE_FACTOR", QByteArray::number(ratio).constData(), 1);
    }

    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        setenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1", 1);
    }
}

int main(int argc, char* argv[])
{
    //qputenv("QT_WAYLAND_SHELL_INTEGRATION", "kwayland-shell");
    DGuiApplicationHelper::setUseInactiveColorGroup(false);
    // load dpi settings
    if (!QFile::exists("/etc/lightdm/deepin/qt-theme.ini")) {
        set_auto_QT_SCALE_FACTOR();
    }
    else {
        DApplication::customQtThemeConfigPath("/etc/lightdm/");

        //在xcb中上一段代码通过读取配置文件能够影响greeter的缩放比，但是在wayland下无效，
        //主线那边通过这种方法会有其它问题，在修改方案，通过配置xsettings去读取缩放比，但是还未完成。
        //klu临时方案：使用QSettgings直接读取配置文件，通过环境变量设置缩放比，后面跟随主线一起修改。
        QSettings settings("/etc/lightdm/deepin/qt-theme.ini", QSettings::IniFormat);
        settings.beginGroup("Theme");
        const double ratio = settings.value("ScreenScaleFactors", 0.0).toDouble();
        if (ratio > 0.0) {
            setenv("QT_SCALE_FACTOR", QByteArray::number(ratio).constData(), 1);
        }
    }

    DApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    qApp->setOrganizationName("deepin");
    qApp->setApplicationName("lightdm-deepin-greeter");
    qApp->setApplicationVersion("2015.1.0");
    qApp->setAttribute(Qt::AA_ForceRasterWidgets);

    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    DPalette pa = DGuiApplicationHelper::instance()->applicationPalette();
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




    SessionBaseModel *model = new SessionBaseModel(SessionBaseModel::AuthType::LightdmType);
    GreeterWorkek *worker = new GreeterWorkek(model); //

    QString locale;
    QTranslator translator;
    if (model->currentUser()) {
        locale = "/usr/share/dde-session-shell/translations/dde-session-shell_" + model->currentUser()->locale().split(".").first();
    }
    if (!QFileInfo::exists(locale)) {
        locale = "/usr/share/dde-session-shell/translations/dde-session-shell_" + QLocale::system().name();
    }
    qDebug()<<"lightdm-deepin-greeter: Current language path : "<<locale;
    translator.load(locale);
    a.installTranslator(&translator);
    QObject::connect(model, &SessionBaseModel::authFinished, model, [=] {
        set_rootwindow_cursor();
    });

    PropertyGroup *property_group = new PropertyGroup(worker);

    property_group->addProperty("contentVisible");

    auto createFrame = [&] (QScreen *screen) -> QWidget* {
        LoginWindow *loginFrame = new LoginWindow(model);
        property_group->addObject(loginFrame);
        loginFrame->setScreen(screen);
        QObject::connect(loginFrame, &LoginWindow::requestSwitchToUser, worker, &GreeterWorkek::switchToUser);
        QObject::connect(loginFrame, &LoginWindow::requestAuthUser, worker, &GreeterWorkek::authUser);
        QObject::connect(loginFrame, &LoginWindow::requestSetLayout, worker, &GreeterWorkek::setLayout);
        QObject::connect(worker, &GreeterWorkek::requestUpdateBackground, loginFrame, static_cast<void (LoginWindow::*)(const QString &)>(&LoginWindow::updateBackground));
        QObject::connect(loginFrame, &LoginWindow::destroyed, property_group, &PropertyGroup::removeObject);
        loginFrame->showFullScreen();
        return loginFrame;
    };

    GreeterScreenManager multi_screen_manager;
    multi_screen_manager.setSessionBaseModel(model);
    multi_screen_manager.register_for_mutil_screen(createFrame);
    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &GreeterScreenManager::startRaiseContentFrame);

    return a.exec();
}
