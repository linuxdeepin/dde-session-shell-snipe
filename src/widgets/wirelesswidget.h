/*
  * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
  *
  * Author:     dengbo <dengbo@uniontech.com>
  *
  * Maintainer: dengbo <dengbo@uniontech.com>
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

#ifndef WIRELESSWIDGET
#define WIRELESSWIDGET

#include <QFrame>

#include <functional>

#include "wirelesspage.h"
#include "src/global_util/util_updateui.h"
#include "src/global_util/wireless/networkworker.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/framedatabind.h"
#include <dboxwidget.h>
#include <DFrame>

DWIDGET_USE_NAMESPACE

class WirelessWidget: public DFrame
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
    explicit WirelessWidget(const QString locale, QWidget *parent = nullptr);
    static WirelessWidget *getInstance(const QString locale);
    ~WirelessWidget() override;

    inline QPointer<dtk::wireless::WirelessPage> getWirelessPagePtr() {return m_wirelessPage;}

    void setModel(SessionBaseModel *const model);

signals:
    void abortOperation();
    void signalStrengthChanged(int signalLevel);
    void hideWirelessList();
    void clicked();

public:
    void updateLocale(std::shared_ptr<User> user);

private Q_SLOTS:
    void onDeviceChanged();

private:
    void init();
    void initConnect(QPointer<dtk::wireless::WirelessPage> wirelessPage);

private:
    SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;
    dtk::wireless::NetworkWorker *m_networkWorker;
    QPointer<dtk::wireless::WirelessPage> m_wirelessPage;
    QVBoxLayout *m_mainLayout = nullptr;
    QString m_localeName;
    DVBoxWidget *m_boxWidget;
};
#endif // WIRELESSWIDGET

