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

#include "loginwindow.h"
#include "constants.h"
#include "greeterworkek.h"
#include "sessionbasemodel.h"
#include "propertygroup.h"
#include "multiscreenmanager.h"

#include <DApplication>
#include <QtCore/QTranslator>
#include <QSettings>

#include <DLog>
#include <DGuiApplicationHelper>
#include <DPlatformTheme>

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

static void set_auto_QT_SCALE_FACTOR() {
    const double ratio = get_scale_ratio();
    if (ratio > 0.0) {
        setenv("QT_SCALE_FACTOR", QByteArray::number(ratio).constData(), 1);
    }

    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        setenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1", 1);
    }
}

static void init()
{
    SessionBaseModel *model = new SessionBaseModel(SessionBaseModel::AuthType::LightdmType);
    GreeterWorkek *worker = new GreeterWorkek(model);

#ifndef QT_DEBUG
    if (!worker->isConnectSync())
        exit(1);
#endif

    if (model->currentUser()) {
        QTranslator translator;
        translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + model->currentUser()->locale().split(".").first());
        DApplication::installTranslator(&translator);
    }

    QObject::connect(model, &SessionBaseModel::authFinished, model, [=](bool is_success) {
        if (is_success)
            set_rootwindow_cursor();
    });

    PropertyGroup *property_group = new PropertyGroup(worker);

    property_group->addProperty("contentVisible");

    auto createFrame = [&](QScreen *screen) -> QWidget * {
        LoginWindow *loginFrame = new LoginWindow(model);
        loginFrame->setScreen(screen);
        property_group->addObject(loginFrame);
        QObject::connect(loginFrame, &LoginWindow::requestSwitchToUser, worker, &GreeterWorkek::switchToUser);
        // QObject::connect(loginFrame, &LoginWindow::requestAuthUser, worker, &GreeterWorkek::authUser);
        QObject::connect(loginFrame, &LoginWindow::requestSetLayout, worker, &GreeterWorkek::setLayout);
        QObject::connect(worker, &GreeterWorkek::requestUpdateBackground, loginFrame, static_cast<void (LoginWindow::*)(const QString &)>(&LoginWindow::updateBackground));
        QObject::connect(loginFrame, &LoginWindow::destroyed, property_group, &PropertyGroup::removeObject);

        QObject::connect(loginFrame, &LoginWindow::requestCheckAccount, worker, &GreeterWorkek::checkAccount);
        QObject::connect(loginFrame, &LoginWindow::requestStartAuthentication, worker, &GreeterWorkek::startAuthentication);
        QObject::connect(loginFrame, &LoginWindow::sendTokenToAuth, worker, &GreeterWorkek::sendTokenToAuth);
        QObject::connect(model, &SessionBaseModel::visibleChanged, loginFrame, &LoginWindow::setVisible);
        return loginFrame;
    };

    MultiScreenManager multi_screen_manager;
    multi_screen_manager.register_for_mutil_screen(createFrame);
    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);
    model->setVisible(true);
}

int main(int argc, char* argv[])
{
    //for qt5platform-plugins load DPlatformIntegration or DPlatformIntegrationParent
    if (!QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin")){
        setenv("XDG_CURRENT_DESKTOP", "Deepin", 1);
    }

    DGuiApplicationHelper::setAttribute(DGuiApplicationHelper::UseInactiveColorGroup, false);
    // load dpi settings
    if (!QFile::exists("/etc/lightdm/deepin/xsettingsd.conf")) {
        set_auto_QT_SCALE_FACTOR();
    }

    DApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    qApp->setOrganizationName("deepin");
    qApp->setApplicationName("lightdm-deepin-greeter");
    qApp->setApplicationVersion("2015.1.0");
    qApp->setAttribute(Qt::AA_ForceRasterWidgets);

    // crash catch
    init_sig_crash();

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

    // follow system active color
    QObject::connect(DGuiApplicationHelper::instance()->systemTheme(), &DPlatformTheme::activeColorChanged, [] (const QColor &color) {
        auto palette = DGuiApplicationHelper::instance()->applicationPalette();
        palette.setColor(QPalette::Highlight, color);
        DGuiApplicationHelper::instance()->setApplicationPalette(palette);
    });

    DLogManager::registerConsoleAppender();

    const QString serviceName = "com.deepin.daemon.Accounts";
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    if (interface->isServiceRegistered(serviceName)) {
        init();
    } else {
        qWarning() << "Accounts service is not registered!";
        QDBusServiceWatcher *serviceWatcher = new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus());
        QObject::connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, &a, [serviceWatcher](const QString &service) {
            qInfo() << "Accounts service is ready" << service;
            init();
            serviceWatcher->deleteLater();
        });
    }

    return a.exec();
}
