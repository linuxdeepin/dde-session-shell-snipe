// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "constants.h"
#include "dbusconstant.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonParseError>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QProcess>

#include <DConfig>

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <cmath>
#include <iostream>

DCORE_USE_NAMESPACE

Q_LOGGING_CATEGORY(DDE_SHELL, "org.deepin.dde.shell")

const bool IsWayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");

// Load system cursor --end

bool isScaleConfigExists() {
    QDBusInterface configInter(DSS_DBUS::systemDisplayService,
                                                     DSS_DBUS::systemDisplayPath,
                                                     DSS_DBUS::systemDisplayService,
                                                    QDBusConnection::systemBus());
    if (!configInter.isValid()) {
        return false;
    }
    QDBusReply<QString> configReply = configInter.call("GetConfig");
    if (configReply.isValid()) {
        QString config = configReply.value();
        QJsonParseError jsonError;
        QJsonDocument jsonDoc(QJsonDocument::fromJson(config.toStdString().data(), &jsonError));
        if (jsonError.error == QJsonParseError::NoError) {
            QJsonObject rootObj = jsonDoc.object();
            QJsonObject Config = rootObj.value("Config").toObject();
            return !Config.value("ScaleFactors").toObject().isEmpty();
        } else {
            return false;
        }
    } else {
        qCWarning(DDE_SHELL) << "Call `GetConfig` failed: " << configReply.error().message();
        return false;
    }
}

// 参照后端算法，保持一致
float toListedScaleFactor(float s)  {
	const float min = 1.0, max = 3.0, step = 0.25;
	if (s <= min) {
		return min;
	} else if (s >= max) {
		return max;
	}

	for (float i = min; i <= max; i += step) {
		if (i > s) {
			float ii = i - step;
			float d1 = s - ii;
			float d2 = i - s;
			if (d1 >= d2) {
				return i;
			} else {
				return ii;
			}
		}
	}
	return max;
}

static double calcMaxScaleFactor(unsigned int width, unsigned int height) {
    const QList<double> scaleFactors = {1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3.0};

    // 如果是竖屏，则反置width, height，防止计算的缩放有误
    if (width < height) {
        std::swap(width, height);
    }

    double maxWScale = width / 1024.0;
    double maxHScale = height / 768.0;

    double maxValue = 0.0;
    if (maxWScale < maxHScale) {
        maxValue = maxWScale;
    } else {
        maxValue = maxHScale;
    }

    if (maxValue > 3.0) {
        maxValue = 3.0;
    }

    double maxScale = 1.0;
    for (int idx = 0; (idx*0.25 + 1.0) <= maxValue; idx++) {
        maxScale = scaleFactors[idx];
    }

    return maxScale;
}

static QStringList getScaleList()
{
    qDebug() << "Get scale list";
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "Display is null";
        return QStringList{};
    }

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));
    if (!resources) {
        resources = XRRGetScreenResources(display, DefaultRootWindow(display));
        qCWarning(DDE_SHELL) << "Get current XRR screen resources failed, instead of getting XRR screen resources, resources: " << resources;
    }

    static const float MinScreenWidth = 1024.0f;
    static const float MinScreenHeight = 768.0f;
    static const QStringList tvstring = {"1.0", "1.25", "1.5", "1.75", "2.0", "2.25", "2.5", "2.75", "3.0"};
    QStringList fscaleList;

    if (resources) {
        for (int i = 0; i < resources->noutput; i++) {
            XRROutputInfo* outputInfo = XRRGetOutputInfo(display, resources, resources->outputs[i]);
            if (outputInfo->crtc == 0 || outputInfo->mm_width == 0)
                continue;

            XRRCrtcInfo *crtInfo = XRRGetCrtcInfo(display, resources, outputInfo->crtc);
            if (crtInfo == nullptr)
                continue;

            auto maxWScale = crtInfo->width / MinScreenWidth;
            auto maxHScale = crtInfo->height / MinScreenHeight;
            auto maxScale = maxWScale < maxHScale ? maxWScale : maxHScale;
            maxScale = maxScale < 3.0f ? maxScale : 3.0f;
            if (fscaleList.isEmpty()) {
                for (int idx = 0; idx * 0.25f + 1.0f <= maxScale; ++idx) {
                    fscaleList << tvstring[idx];
                }
                qDebug() << "First screen scale list:" << fscaleList;
            } else {
                QStringList tmpList;
                for (int idx = 0; idx * 0.25f + 1.0f <= maxScale; ++idx) {
                    tmpList << tvstring[idx];
                }
                qDebug()  << "Current screen scale list:" << tmpList;
                // fscaleList、tmpList两者取交集
                for (const auto &scale : fscaleList) {
                    if (!tmpList.contains(scale))
                        fscaleList.removeAll(scale);
                }
            }
        }
    } else {
        qCWarning(DDE_SHELL) << "Get scale factor failed, please check X11 Extension";
    }

    return fscaleList;
}

static double getScaleFactor() {
    Display *display = XOpenDisplay(nullptr);
    double scaleFactor = 0.0;

    if (!display) {
        return scaleFactor;
    }

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));

    if (!resources) {
        resources = XRRGetScreenResources(display, DefaultRootWindow(display));
        qCWarning(DDE_SHELL) << "Get current XRR screen resources failed, instead of getting XRR screen resources, resources: " << resources;
    }

    if (resources) {
        double minScaleFactor = 3.0;
        for (int i = 0; i < resources->noutput; i++) {
            XRROutputInfo* outputInfo = XRRGetOutputInfo(display, resources, resources->outputs[i]);
            if (outputInfo->crtc == 0 || outputInfo->mm_width == 0) continue;

            XRRCrtcInfo *crtInfo = XRRGetCrtcInfo(display, resources, outputInfo->crtc);
            if (crtInfo == nullptr) continue;
            // 参照后端的算法，与后端保持一致
            float lenPx = hypot(static_cast<double>(crtInfo->width), static_cast<double>(crtInfo->height));
            float lenMm = hypot(static_cast<double>(outputInfo->mm_width), static_cast<double>(outputInfo->mm_height));
            float lenPxStd = hypot(1920, 1080);
            float lenMmStd = hypot(477, 268);
            float fix = (lenMm - lenMmStd) * (lenPx / lenPxStd) * 0.00158;
            if (fix > 0.5) {
                fix = 0.5;
            }
            if (fix < -0.5) {
                fix = -0.5;
            }
            float scale = (lenPx / lenMm) / (lenPxStd / lenMmStd) + fix;
            scaleFactor = toListedScaleFactor(scale);

            double maxScaleFactor = calcMaxScaleFactor(crtInfo->width, crtInfo->height);
            if (scaleFactor > maxScaleFactor) {
                scaleFactor = maxScaleFactor;
            }
            if (minScaleFactor > scaleFactor) {
                minScaleFactor = scaleFactor;
            }
        }
        scaleFactor = scaleFactor > 0.0 ? minScaleFactor : 1;
    }
    else {
        qCWarning(DDE_SHELL) << "Get scale factor failed, please check X11 Extension";
    }

    return scaleFactor;
}

// 读取系统配置文件的缩放比，如果是null，默认返回1
double getScaleFormConfig()
{
    double defaultScaleFactor = 1.0;
    DTK_CORE_NAMESPACE::DConfig * dconfig = DConfig::create("org.deepin.dde.lightdm-deepin-greeter", "org.deepin.dde.lightdm-deepin-greeter", QString(), nullptr);
    //华为机型,从override配置中获取默认缩放比
    if (dconfig) {
        defaultScaleFactor = dconfig->value("defaultScaleFactors", 1.0).toDouble() ;
        qDebug() <<"Default scale factor: " << defaultScaleFactor;
        if(defaultScaleFactor < 1.0){
            defaultScaleFactor = 1.0;
        }

        delete dconfig;
        dconfig = nullptr;
    }

    QDBusInterface configInter(DSS_DBUS::systemDisplayService,
                                                     DSS_DBUS::systemDisplayPath,
                                                     DSS_DBUS::systemDisplayService,
                                                    QDBusConnection::systemBus());
    if (!configInter.isValid()) {
        return defaultScaleFactor;
    }
    QDBusReply<QString> configReply = configInter.call("GetConfig");
    if (configReply.isValid()) {
        QString config = configReply.value();
        QJsonParseError jsonError;
        QJsonDocument jsonDoc(QJsonDocument::fromJson(config.toLatin1(), &jsonError));
        if (jsonError.error == QJsonParseError::NoError) {
            QJsonObject rootObj = jsonDoc.object();
            QJsonObject Config = rootObj.value("Config").toObject();
            double scaleFactor = Config.value("ScaleFactors").toObject().value("ALL").toDouble();
            qDebug() << "Scale factor from system display config: " << scaleFactor;
            if(scaleFactor == 0.0) {
                scaleFactor = defaultScaleFactor;
            } else {
                // 处理关机期间从高分屏换到低分屏的场景
                const auto &scales = getScaleList();
                qDebug() << "Scales:" << scales;
                if (!scales.isEmpty() && !scales.contains(QString::number(scaleFactor, 'f', 2).replace("00", "0"))) {
                    qInfo() << "Scale factor is not in scales, use default scale factor";
                    scaleFactor = defaultScaleFactor;
                }
            }
            return scaleFactor;
        } else {
            return defaultScaleFactor;
        }
    } else {
        qCWarning(DDE_SHELL) << "DBus call `GetConfig` failed, reply is invaild, error: " << configReply.error().message();
        return defaultScaleFactor;
    }
}

static void setQtScaleFactorEnv() {
    const double scaleFactor = IsWayland ? getScaleFormConfig() : getScaleFactor();
    qDebug() << "Final scale factor: " << scaleFactor;
    if (scaleFactor > 0.0) {
        std::cout << QString("QT_SCALE_FACTOR="+QByteArray::number(scaleFactor)).toStdString().c_str() << std::endl;
    } else {
        std::cout << QString("QT_AUTO_SCREEN_SCALE_FACTOR=1").toStdString().c_str() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // x11下，dxcb设置Qt::AA_EnableHighDpiScaling属性后，平台插件会处理缩放，不需要再主动设置QT_SCALE_FACTOR，否则会导致缩放系数相乘，得出错误的qApp->devicePixelRatio()
    // wayland下，dwayland平台插件不会处理缩放，需要手动设置QT_SCALE_FACTOR
    if (IsWayland) {
        setQtScaleFactorEnv();
    }

    return 0;
}
