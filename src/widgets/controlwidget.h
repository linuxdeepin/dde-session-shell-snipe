/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <DFloatingButton>
#include "mediawidget.h"

DWIDGET_USE_NAMESPACE

class ControlWidget : public QWidget
{
    Q_OBJECT

    enum WiFiStrenthLevel {
        WiFiStrengthNoNE,
        WiFiStrengthNoLevel,
        WiFiStrengthLOWLevel,
        WiFiStrengthMidLevel,
        WiFiStrengthMidHighLevel,
        WiFiStrengthHighLevel,
    };
public:
    explicit ControlWidget(QWidget *parent = nullptr);
    inline bool getWirelessDeviceExistFlg() {return m_wifiDeviceExist;}

signals:
    void requestSwitchUser();
    void requestShutdown();
    void requestSwitchSession();
    void requestSwitchVirtualKB();
    void requestWiFiPage();
    void updateWirelessDisplay();

public slots:
    void setVirtualKBVisible(bool visible);
    void setUserSwitchEnable(const bool visible);
    void setWirelessListEnable(const bool visible);
    void setSessionSwitchEnable(const bool visible);
    void updateWirelessBtnDisplay(const QString &path);
    void chooseToSession(const QString &session);
    void leftKeySwitch();
    void rightKeySwitch();
    void updateWifiIconDisplay(int wifiLevel);

protected:
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent *event) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void initConnect();
    void showTips();
    void hideTips();

private:
    enum FocusState {
        FocusNo,
        FocusHasIn,
        FocusReadyOut
    };
    FocusState m_focusState = FocusNo;

    int m_index = 0;
    QList<DFloatingButton *> m_btnList;

    QHBoxLayout *m_mainLayout = nullptr;
    DFloatingButton *m_virtualKBBtn = nullptr;
    DFloatingButton *m_wirelessBtn = nullptr;
    DFloatingButton *m_switchUserBtn = nullptr;
    DFloatingButton *m_powerBtn = nullptr;
    DFloatingButton *m_sessionBtn = nullptr;
    QLabel *m_sessionTip = nullptr;
    QWidget *m_tipWidget = nullptr;
    bool m_wifiDeviceExist = false;
#ifndef SHENWEI_PLATFORM
    QPropertyAnimation *m_tipsAni = nullptr;
#endif
};

#endif // CONTROLWIDGET_H
