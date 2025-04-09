// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <DSysInfo>

#include <QString>
#include <QStringList>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(DDE_SHELL)
namespace DDESESSIONCC
{

static const QString CONFIG_FILE("/var/lib/AccountsService/deepin/users/");
static const QString DEFAULT_CURSOR_THEME("/usr/share/icons/default/index.theme");
static const QString LAST_USER_CONFIG("/var/lib/lightdm/lightdm-deepin-greeter");
static const int PASSWD_EDIT_WIDTH = 280;
static const int PASSWD_EDIT_HEIGHT = 36;
static const int LOCK_CONTENT_TOP_WIDGET_HEIGHT = 132; // 顶部控件（日期）的高度
static const int LOCK_CONTENT_CENTER_LAYOUT_MARGIN = 33; // SessionBaseWindow 中main layout的上下间隔
static const int BIO_AUTH_STATE_PLACE_HOLDER_HEIGHT = 42; // 生物认证状态占位高度
static const int CHOOSE_AUTH_TYPE_PLACE_HOLDER_HEIGHT = 36; // 认证类型选择按钮占位高度
static const int BIO_AUTH_STATE_BOTTOM_SPACING = 30; // 生物识别状态底部间隔
static const int CHOOSE_AUTH_TYPE_BUTTON_BOTTOM_SPACING = 40; // 认证类型选择按钮底部间隔
static const double AUTH_WIDGET_TOP_SPACING_PERCENT = 0.35; // 认证窗口顶部间隔占整个屏幕的百分比
static const int LAYOUT_BUTTON_HEIGHT =  34;
static const int KEYBOARD_LAYOUT_WIDTH = 200;

static const int CAPS_LOCK_STATUS_WIDTH = 23;
static const int CAPS_LOCK_STATUS_RIGHT_MARGIN = 8;
static const int BASE_SCREEN_HEIGHT = 1080;
static const int MIN_AUTH_WIDGET_HEIGHT = 276; // 密码验证时整个登录控件的大小，默认为最小高度（不考虑无密码登录的情况，稍许影响）

const QStringList session_ui_configs {
    "/etc/lightdm/lightdm-deepin-greeter.conf",
    "/etc/deepin/dde-session-ui.conf",
    "/usr/share/dde-session-shell/dde-session-shell.conf",
    "/usr/share/dde-session-ui/dde-session-ui.conf"
};

const QStringList SHUTDOWN_CONFIGS {
    "/etc/lightdm/lightdm-deepin-greeter.conf",
    "/etc/deepin/dde-session-ui.conf",
    "/etc/deepin/dde-shutdown.conf",
    "/usr/share/dde-session-ui/dde-shutdown.conf"
};

static const QString SOLID_BACKGROUND_COLOR = "#000F27";        // 纯色背景色号
static const QString LOCK_DCONFIG_SOURCE = "org.deepin.dde.lock"; // 锁屏配置文件
static const QString LOGIN_DCONFIG_SOURCE = "org.deepin.dde.lightdm-deepin-greeter"; // 登录配置文件

enum AuthFactorType {
    SingleAuthFactor,
    MultiAuthFactor,
};

using namespace Dtk::Core;

const DSysInfo::UosEdition UosEdition = DSysInfo::uosEditionType();
const bool IsCommunitySystem = (DSysInfo::UosCommunity == UosEdition);//是否是社区版

// dconfig 配置名称
const QString USE_SOLID_BACKGROUND = QStringLiteral("useSolidBackground");
const QString AUTO_EXIT = QStringLiteral("autoExit");
const QString HIDE_LOGOUT_BUTTON = QStringLiteral("hideLogoutButton");
const QString HIDE_ONBOARD = QStringLiteral("hideOnboard");
const QString ENABLE_ONE_KEY_LOGIN = QStringLiteral("enableOneKeylogin");
const QString ENABLE_SHORTCUT_FOR_LOCK = QStringLiteral("enableShortcutForLock");
const QString SHOW_TOP_TIP = QStringLiteral("showTopTip");
const QString TOP_TIP_TEXT = QStringLiteral("topTipText");
const QString TOP_TIP_TEXT_FONT = QStringLiteral("topTipTextFont");
const QString SHOW_USER_NAME = QStringLiteral("showUserName");
const QString CHANGE_PASSWORD_FOR_NORMAL_USER = QStringLiteral("changePasswordForNormalUser");
const QString USER_FRAME_MAX_WIDTH = QStringLiteral("userFrameMaxWidth");
const QString FULL_NAME_FONT = QStringLiteral("fullNameFont");
const QString LOGIN_PLUGIN_DISPLAY_ORDER = QStringLiteral("loginPluginsDisplayOrder");
const QString LOGIN_PLUGIN_AUTH_ORDER = QStringLiteral("loginPluginsAuthOrder");
const QString SHOW_MEDIA_WIDGET = "showMediaWidget";

// 系统版本显示配置
const QString SHOW_SYSTEM_VERSION = QStringLiteral("showSystemVersion");
// 第三方logo相关配置
const QString CUSTOM_LOGO_PATH = QStringLiteral("customLogoPath");
const QString CUSTOM_LOGO_POS = QStringLiteral("customLogoPos");
}


#endif // CONSTANTS_H
