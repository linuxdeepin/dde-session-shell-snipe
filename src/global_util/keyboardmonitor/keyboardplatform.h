// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYBOARDPLATFORM_H
#define KEYBOARDPLATFORM_H

#include <QObject>

class KeyBoardPlatform : public QObject
{
    Q_OBJECT
public:
    KeyBoardPlatform(QObject *parent = nullptr);
    virtual ~KeyBoardPlatform();

    virtual bool isCapsLockOn() = 0;
    virtual bool isNumLockOn() = 0;
    virtual bool setNumLockStatus(const bool &on) = 0;
    virtual void run() = 0;
    virtual void ungrabKeyboard() = 0;

signals:
    void capsLockStatusChanged(bool on);
    void numLockStatusChanged(bool on);
    void initialized();
};

#endif // KEYBOARDPLATFORM_H
