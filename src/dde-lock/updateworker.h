// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UPDATECTRL_H
#define UPDATECTRL_H

#include "systempower1interface.h"

using PowerInter = org::deepin::dde::Power1;

class UpdateWorker : public QObject
{
    Q_OBJECT

public:
    static UpdateWorker *instance() {
        static UpdateWorker *pIns = nullptr;
        if (!pIns) {
            pIns = new UpdateWorker();
        }
        return pIns;
    };

    void doUpdate(bool powerOff);

Q_SIGNALS:
    void requestExitUpdating();

private:
    explicit UpdateWorker(QObject *parent = nullptr);

    bool checkPower();
    void prepareFullScreenUpgrade(bool powerOff);
};

#endif // UPDATECTRL_H
