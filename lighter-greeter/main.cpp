// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QScreen>

#include <DLog>
#include <DGuiApplicationHelper>

#include "lightergreeter.h"
#include "lighterbackground.h"
#include "lighterscreenmanager.h"
#include "public_func.h"

DCORE_USE_NAMESPACE
DGUI_USE_NAMESPACE

int main(int argc, char *argv[])
{
    // 正确加载dxcb插件
    //for qt5platform-plugins load DPlatformIntegration or DPlatformIntegrationParent
    if (!QString(qgetenv("XDG_CURRENT_DESKTOP")).toLower().startsWith("deepin")) {
        setenv("XDG_CURRENT_DESKTOP", "Deepin", 1);
    }

    DGuiApplicationHelper::setAttribute(DGuiApplicationHelper::UseInactiveColorGroup, false);

    // qt默认当最后一个窗口析构后，会自动退出程序，这里设置成false，防止插拔时，没有屏幕，导致进程退出
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    qApp->setOrganizationName("deepin");
    qApp->setApplicationName("org.deepin.dde.lightdm-deepin-greeter");
    setAppType(APP_TYPE_LOGIN);
    qApp->setProperty("dssAppType", APP_TYPE_LOGIN);

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

    setPointer();

    auto createFrame = [&](QPointer<QScreen> screen) -> QWidget * {
        // 所有的界面共用一块内容
        static LighterGreeter *g_greeter = new LighterGreeter;

        LighterBackground *bg = new LighterBackground(g_greeter);
        QObject::connect(g_greeter, &LighterGreeter::authFinished, bg, &LighterBackground::onAuthFinished);
        bg->setScreen(screen);
        return bg;
    };

    new LighterScreenManager(createFrame);

    return a.exec();
}
