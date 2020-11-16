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

#include <DApplication>
#include <QtCore/QTranslator>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDBusAbstractInterface>
#include <DDBusSender>
#include <DGuiApplicationHelper>
#include <DPlatformTheme>

#include <DLog>
#include <unistd.h>

#include "src/dde-shutdown/app/shutdownframe.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/dde-shutdown/shutdownworker.h"
#include "src/widgets/propertygroup.h"
#include "src/global_util/multiscreenmanager.h"

#include "src/dde-shutdown/dbusshutdownagent.h"

const QString DBUS_PATH = "/com/deepin/dde/shutdownFront";
const QString DBUS_NAME = "com.deepin.dde.shutdownFront";

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

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    app->setOrganizationName("deepin");
    app->setApplicationName("dde-shutdown");

    // crash catch
    init_sig_crash();

    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    DPalette pa = DGuiApplicationHelper::instance()->applicationPalette();
    pa.setColor(QPalette::Normal, DPalette::WindowText, QColor("#FFFFFF"));
    pa.setColor(QPalette::Normal, DPalette::AlternateBase, QColor(0, 0, 0, 76));
    pa.setColor(QPalette::Normal, DPalette::Button, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Light, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Dark, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::ButtonText, QColor("#FFFFFF"));
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::WindowText, DGuiApplicationHelper::LightType);
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

    QTranslator translator;
    translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + QLocale::system().name());
    app->installTranslator(&translator);

    QCommandLineOption showOption(QStringList() << "s" << "show", "show dde-shutdown(hide for default).");
    QCommandLineOption backend(QStringList() << "d" << "daemon", "start to daemon mode");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(showOption);
    parser.addOption(backend);
    parser.process(*app);

    QDBusConnection session = QDBusConnection::sessionBus();
    if (!session.registerService(DBUS_NAME) ||
            !app->setSingleInstance(QString("dde-shutdown%1").arg(getuid()), DApplication::UserScope)) {
        qWarning() << "dde-shutdown is running...";

        if (parser.isSet(showOption)) {
            DDBusSender()
            .service(DBUS_NAME)
            .interface(DBUS_NAME)
            .path(DBUS_PATH)
            .method("Show")
            .call();
        }

        return 0;
    } else {
        qDebug() << "dbus registration success.";

        SessionBaseModel *model = new SessionBaseModel(SessionBaseModel::AuthType::UnknowAuthType);
        ShutdownWorker *worker = new ShutdownWorker(model); //

        DBusShutdownAgent *dbusAgent = new DBusShutdownAgent;
        PropertyGroup *property_group = new PropertyGroup(worker);

        property_group->addProperty("contentVisible");

        ShutdownFrontDBus adaptor(dbusAgent, model);

        auto createFrame = [&](QScreen * screen) -> QWidget* {
            ShutdownFrame *frame = new ShutdownFrame(model);
            dbusAgent->addFrame(frame);
            frame->setScreen(screen);
            property_group->addObject(frame);
            QObject::connect(model, &SessionBaseModel::visibleChanged, frame, &ShutdownFrame::setVisible);
            QObject::connect(frame, &ShutdownFrame::requestEnableHotzone, worker, &ShutdownWorker::enableZoneDetected);
            QObject::connect(frame, &ShutdownFrame::destroyed, property_group, &PropertyGroup::removeObject);
            QObject::connect(frame, &ShutdownFrame::destroyed, frame, [ = ] {
                dbusAgent->removeFrame(frame);
            });
            QObject::connect(frame, &ShutdownFrame::buttonClicked, frame, [ = ](const Actions action) {
                dbusAgent->sync(action);
            });
            QObject::connect(frame, &ShutdownFrame::sendKeyValue, frame, [&](QString key) {
                emit adaptor.ChangKey(key);
            });
            frame->setVisible(model->isShow());
            return frame;
        };

        MultiScreenManager multi_screen_manager;
        multi_screen_manager.register_for_mutil_screen(createFrame);

        QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);

        if (parser.isSet(showOption)) {
            model->setIsShow(true);
        }

        #ifdef QT_DEBUG
            model->setIsShow(true);
        #endif

        Q_UNUSED(adaptor);
        QDBusConnection::sessionBus().registerObject(DBUS_PATH, dbusAgent);

        return app->exec();
    }
}
