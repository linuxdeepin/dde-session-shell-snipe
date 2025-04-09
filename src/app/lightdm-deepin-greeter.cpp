// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "accessibilitycheckerex.h"
#include "appeventfilter.h"
#include "constants.h"
#include "greeterworker.h"
#include "logincontent.h"
#include "loginwindow.h"
#include "modules_loader.h"
#include "multiscreenmanager.h"
#include "sessionbasemodel.h"
#include "public_func.h"
#include "plugin_manager.h"

#ifndef ENABLE_DSS_SNIPE
#include <dde-api/eventlogger.hpp>
#endif
#include <DApplication>
#include <DGuiApplicationHelper>
#include <DLog>

#include <QGuiApplication>
#include <QScreen>
#include <QSurfaceFormat>

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <cmath>

#include "dbusconstant.h"

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

const bool IsWayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");

int main(int argc, char* argv[])
{
    // 正确加载dxcb插件
    //for qt5platform-plugins load DPlatformIntegration or DPlatformIntegrationParent
    if (!QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin")){
        setenv("XDG_CURRENT_DESKTOP", "Deepin", 1);
    }

    // 配置 qwebengine
    configWebEngine();

    DGuiApplicationHelper::setAttribute(DGuiApplicationHelper::UseInactiveColorGroup, false);

    // 以下4行为解决登录和锁屏的默认字体不一致的情况，gsettings默认值为10.5，
    // 而登录读取不到gsettings配置的默认值而使用Qt默认的9，导致登录界面字体很小。
    // bug:161915
    QFont font;
    font.setPointSize(10.5);
    font.setFamily("Noto Sans CJK SC-Thin");
    qGuiApp->setFont(font);

    DApplication a(argc, argv);

#ifndef ENABLE_DSS_SNIPE
    DDE_EventLogger::EventLogger::instance().init("org.deepin.dde.lightdm-deepin-greeter", false);
#endif
    // qt默认当最后一个窗口析构后，会自动退出程序，这里设置成false，防止插拔时，没有屏幕，导致进程退出
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    qApp->setOrganizationName("deepin");
    qApp->setApplicationName("org.deepin.dde.lightdm-deepin-greeter");

    setPointer();

    //注册全局事件过滤器
    AppEventFilter appEventFilter;
    a.installEventFilter(&appEventFilter);
    setAppType(APP_TYPE_LOGIN);
    qApp->setProperty("dssAppType", APP_TYPE_LOGIN);

    DGuiApplicationHelper::instance()->setSizeMode(DGuiApplicationHelper::SizeMode::NormalMode);
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

    DLogManager::setLogFormat("%{time}{yyyy-MM-dd, HH:mm:ss.zzz} [%{type:-7}] [ %{function:-35} %{line}] %{message}\n");
    DLogManager::registerConsoleAppender();

    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    if (!interface->isServiceRegistered(DSS_DBUS::accountsService)) {
        qCWarning(DDE_SHELL) << "Accounts service is not registered wait...";
        QDBusServiceWatcher *serviceWatcher = new QDBusServiceWatcher(DSS_DBUS::accountsService, QDBusConnection::systemBus());
        QObject::connect(serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, [](const QString &service) {
            if (service == DSS_DBUS::accountsService) {
                qCritical() << "ERROR: accounts service unregistered";
            }
        });

#ifdef ENABLE_WAITING_ACCOUNTS_SERVICE
        qDebug() << "Waiting for deepin accounts service";
        QEventLoop eventLoop;
        QObject::connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, &eventLoop, &QEventLoop::quit);
#ifdef  QT_DEBUG
        QTimer::singleShot(10000, &eventLoop, &QEventLoop::quit);
#endif
        eventLoop.exec();
        qDebug() << "Service registered!";
#endif
    }

    // 加载login模块
    ModulesLoader::instance().setLoadLoginModule(true);
    ModulesLoader::instance().start(QThread::LowestPriority);

    SessionBaseModel *model = new SessionBaseModel();
    model->setAppType(Login);
    GreeterWorker *worker = new GreeterWorker(model);
    QObject::connect(&appEventFilter, &AppEventFilter::userIsActive, worker, &GreeterWorker::restartResetSessionTimer);

    /* load translation files */
    loadTranslation(model->currentUser()->locale());

    PluginManager::instance()->setModel(model);

    // 设置系统登录成功的加载光标
    QObject::connect(model, &SessionBaseModel::authFinished, model, [=](bool isSuccess) {
        if (isSuccess)
            setRootWindowCursor();
    });

    // 初始化LoginContent
    LoginContent::instance()->init(model);
    QObject::connect(LoginContent::instance(), &LoginContent::requestSwitchToUser, worker, &GreeterWorker::switchToUser);
    QObject::connect(LoginContent::instance(), &LoginContent::requestSetLayout, worker, &GreeterWorker::setKeyboardLayout);
    QObject::connect(LoginContent::instance(), &LoginContent::requestCheckAccount, worker, &GreeterWorker::checkAccount);
    QObject::connect(LoginContent::instance(), &LoginContent::requestCheckSameNameAccount, worker, &GreeterWorker::checkSameNameAccount);
    QObject::connect(worker, &GreeterWorker::requestShowUsersWithTheSameName, LoginContent::instance(), &LoginContent::showUsersWithTheSameName);
    QObject::connect(LoginContent::instance(), &LoginContent::requestStartAuthentication, worker, &GreeterWorker::startAuthentication);
    QObject::connect(LoginContent::instance(), &LoginContent::sendTokenToAuth, worker, &GreeterWorker::sendTokenToAuth);
    QObject::connect(LoginContent::instance(), &LoginContent::requestEndAuthentication, worker, &GreeterWorker::endAuthentication);
    QObject::connect(LoginContent::instance(), &LoginContent::authFinished, worker, &GreeterWorker::onAuthFinished);
    QObject::connect(LoginContent::instance(), &LoginContent::noPasswordLoginChanged, worker, &GreeterWorker::onNoPasswordLoginChanged);

    // 根据屏幕创建全屏背景窗口
    auto createFrame = [&](QPointer<QScreen> screen, int count) -> QWidget * {
        LoginWindow *loginFrame = new LoginWindow(model);
        // 创建Frame可能会花费数百毫秒，这个和机器性能有关，在此过程完成后，screen可能已经析构了
        // 在wayland的环境插拔屏幕或者显卡驱动有问题时可能会出现此类问题
        if (screen.isNull()) {
            loginFrame->deleteLater();
            loginFrame = nullptr;
            qCWarning(DDE_SHELL) << "Screen was released when the frame was created";
            return nullptr;
        }
        loginFrame->setScreen(screen, count <= 0);
        QObject::connect(worker, &GreeterWorker::requestUpdateBackground, loginFrame, &LoginWindow::updateBackground);
        loginFrame->setVisible(model->visible());
        return loginFrame;
    };

    MultiScreenManager multi_screen_manager;
    multi_screen_manager.register_for_multi_screen(createFrame);
    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);

#if defined(DSS_CHECK_ACCESSIBILITY) && defined(QT_DEBUG)
    AccessibilityCheckerEx checker;
    checker.addIgnoreClasses(QStringList()
                             << "Dtk::Widget::DBlurEffectWidget");
    checker.setOutputFormat(DAccessibilityChecker::FullFormat);
    checker.start();
#endif
    model->setVisible(true);

    // 加载非login模块
    ModulesLoader::instance().setLoadLoginModule(false);
    ModulesLoader::instance().start(QThread::LowestPriority);

    return a.exec();
}
