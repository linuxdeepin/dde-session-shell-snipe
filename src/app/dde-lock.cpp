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
#include "dbuslockagent.h"
#include "dbuslockfrontservice.h"
#include "dbusshutdownagent.h"
#include "dbusshutdownfrontservice.h"
#include "lockcontent.h"
#include "lockframe.h"
#include "lockworker.h"
#include "modules_loader.h"
#include "multiscreenmanager.h"
#include "propertygroup.h"
#include "sessionbasemodel.h"

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DLog>
#include <DPlatformTheme>

#include <QDBusInterface>

#include <unistd.h>

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication *app = nullptr;
#if (DTK_VERSION < DTK_VERSION_CHECK(5, 4, 0, 0))
    app = new DApplication(argc, argv);
#else
    app = DApplication::globalApplication(argc, argv);
#endif

    //解决Qt在Retina屏幕上图片模糊问题
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    app->setOrganizationName("deepin");
    app->setApplicationName("org.deepin.dde.lock");
    app->setApplicationVersion("2015.1.0");

    //注册全局事件过滤器
    AppEventFilter appEventFilter;
    app->installEventFilter(&appEventFilter);
    setAppType(APP_TYPE_LOCK);

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

    // follow system active color
    QObject::connect(DGuiApplicationHelper::instance()->systemTheme(), &DPlatformTheme::activeColorChanged, [] (const QColor &color) {
        auto palette = DGuiApplicationHelper::instance()->applicationPalette();
        palette.setColor(QPalette::Highlight, color);
        DGuiApplicationHelper::instance()->setApplicationPalette(palette);
    });

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    /* load translation files */
    loadTranslation(QLocale::system().name());

    dss::module::ModulesLoader *modulesLoader = &dss::module::ModulesLoader::instance();
    modulesLoader->start(QThread::LowestPriority);

    QCommandLineParser cmdParser;
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();

    QCommandLineOption backend(QStringList() << "d" << "daemon", "start to daemon mode");
    cmdParser.addOption(backend);
    QCommandLineOption switchUser(QStringList() << "s" << "switch", "show user switch");
    cmdParser.addOption(switchUser);

    QCommandLineOption shutdown(QStringList() << "t" << "shutdown", "show shut down");
    cmdParser.addOption(shutdown);

    QCommandLineOption lockscreen(QStringList() << "l" << "lockscreen", "show lock screen");
    cmdParser.addOption(lockscreen);

    QStringList xddd = app->arguments();
    cmdParser.process(*app);

    bool runDaemon = cmdParser.isSet(backend);
    bool showUserList = cmdParser.isSet(switchUser);
    bool showShutdown = cmdParser.isSet(shutdown);
    bool showLockScreen = cmdParser.isSet(lockscreen);

#ifdef  QT_DEBUG
    showLockScreen = true;
#endif

    SessionBaseModel *model = new SessionBaseModel();
    model->setAppType(Lock);
    LockWorker *worker = new LockWorker(model);
    QObject::connect(&appEventFilter, &AppEventFilter::userIsActive, worker, &LockWorker::restartResetSessionTimer);
    PropertyGroup *property_group = new PropertyGroup(worker);

    if (worker->isLocked()) {
        //如果当前Session被锁定，则启动dde-lock时直接进入锁屏状态
        //兼容在desktop文件中添加-l启动参数来解决杀掉dde-lock后启动器无法打开的问题,BUG91126
        //同时避免在desktop文件中添加-l启动参数在正常启动时就直接进入锁屏而无法调用任务栏关机按钮中接口的问题,BUG92030
        showLockScreen = true;
    }

    property_group->addProperty("contentVisible");

    DBusLockAgent lockAgent;
    lockAgent.setModel(model);
    DBusLockFrontService lockService(&lockAgent);

    DBusShutdownAgent shutdownAgent;
    shutdownAgent.setModel(model);
    DBusShutdownFrontService shutdownServices(&shutdownAgent);

    auto createFrame = [&] (QScreen *screen, int count) -> QWidget* {
        LockFrame *lockFrame = new LockFrame(model);
        lockFrame->setScreen(screen, count <= 0);
        property_group->addObject(lockFrame);
        QObject::connect(lockFrame, &LockFrame::requestSwitchToUser, worker, &LockWorker::switchToUser);
        QObject::connect(model, &SessionBaseModel::visibleChanged, lockFrame, &LockFrame::setVisible);
        QObject::connect(model, &SessionBaseModel::visibleChanged, lockFrame,[&](bool visible) {
            emit lockService.Visible(visible);
        });
        QObject::connect(lockFrame, &LockFrame::requestSetLayout, worker, &LockWorker::setKeyboardLayout);
        QObject::connect(lockFrame, &LockFrame::requestEnableHotzone, worker, &LockWorker::enableZoneDetected, Qt::UniqueConnection);
        QObject::connect(lockFrame, &LockFrame::destroyed, property_group, &PropertyGroup::removeObject);
        QObject::connect(lockFrame, &LockFrame::sendKeyValue, [&](QString key) {
             emit lockService.ChangKey(key);
        });
        QObject::connect(lockFrame, &LockFrame::requestStartAuthentication, worker, &LockWorker::startAuthentication);
        QObject::connect(lockFrame, &LockFrame::sendTokenToAuth, worker, &LockWorker::sendTokenToAuth);
        QObject::connect(lockFrame, &LockFrame::requestEndAuthentication, worker, &LockWorker::onEndAuthentication);
        QObject::connect(lockFrame, &LockFrame::authFinished, worker, &LockWorker::onAuthFinished);
        if (model->isUseWayland()) {
            QObject::connect(lockFrame, &LockFrame::requestDisableGlobalShortcutsForWayland, worker, &LockWorker::disableGlobalShortcutsForWayland);
        }

        lockFrame->setVisible(model->visible());
        emit lockService.Visible(model->visible());
        return lockFrame;
    };

    MultiScreenManager multi_screen_manager;
    multi_screen_manager.register_for_mutil_screen(createFrame);

#if defined(DSS_CHECK_ACCESSIBILITY) && defined(QT_DEBUG)
    AccessibilityCheckerEx checker;
    //Dtk::Widget::DBlurEffectWidget 添加accessibleName、objectName都会检查出内存泄露，原因不知
    checker.addIgnoreClasses(QStringList()
                          << "Dtk::Widget::DBlurEffectWidget");
    checker.setOutputFormat(DAccessibilityChecker::FullFormat);
    checker.start();
#endif

    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);

    QDBusConnection conn = QDBusConnection::sessionBus();
    int ret = 0;
    if (!conn.registerService(DBUS_LOCK_NAME) ||
        !conn.registerObject(DBUS_LOCK_PATH, &lockAgent) ||
        !conn.registerService(DBUS_SHUTDOWN_NAME) ||
        !conn.registerObject(DBUS_SHUTDOWN_PATH, &shutdownAgent) ||
        !app->setSingleInstance(QString("dde-lock%1").arg(getuid()), DApplication::UserScope)) {
        qDebug() << "register dbus failed"<< "maybe lockFront is running..." << conn.lastError();

        if (!runDaemon) {
            const char *lockFrontInter = "com.deepin.dde.lockFront";
            const char *shutdownFrontInter = "com.deepin.dde.shutdownFront";
            if (showUserList) {
                QDBusInterface ifc(DBUS_LOCK_NAME, DBUS_LOCK_PATH, lockFrontInter, QDBusConnection::sessionBus(), nullptr);
                ifc.asyncCall("ShowUserList");
            } else if (showShutdown) {
                QDBusInterface ifc(DBUS_SHUTDOWN_NAME, DBUS_SHUTDOWN_PATH, shutdownFrontInter, QDBusConnection::sessionBus(), nullptr);
                ifc.asyncCall("Show");
            } else if (showLockScreen) {
                QDBusInterface ifc(DBUS_LOCK_NAME, DBUS_LOCK_PATH, lockFrontInter, QDBusConnection::sessionBus(), nullptr);
                ifc.asyncCall("Show");
            }
        }
    } else {
        if (!runDaemon) {
            if (showUserList) {
                emit model->showUserList();
            } else if (showShutdown) {
                emit model->showShutdown();
            } else if (showLockScreen) {
                emit model->showLockScreen();
            }
        }
        ret = app->exec();
    }
    return ret;
}
