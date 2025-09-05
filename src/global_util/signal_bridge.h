// SPDX-FileCopyrightText: 2016 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIGNALBRIDGE_H
#define SIGNALBRIDGE_H

#include "constants.h"
#include "authcommon.h"

#include <QObject>

#include <DSingleton>

class SignalBridge : public QObject, public Dtk::Core::DSingleton<SignalBridge>
{
    Q_OBJECT
    friend class Dtk::Core::DSingleton<SignalBridge>;
private:
    explicit SignalBridge(QObject *parent = nullptr) : QObject(parent) {}

signals:
    /**
     * @brief 发送 extra info 给deepin-authentication
     */
    void requestSendExtraInfo(const QString &account, const AuthCommon::AuthType authType, const QString &token);
};

#endif // SIGNALBRIDGE_H
