// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSLOCKFRONTSERVICE_H
#define DBUSLOCKFRONTSERVICE_H

#include "dbuslockagent.h"

#include <QDBusAbstractAdaptor>

QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QVariant;
QT_END_NAMESPACE

const QString lockFrontService = "org.deepin.dde.LockFront1";

/*
 * Proxy class for interface com.deepin.dde.lockFront
 */
class DBusLockFrontService: public QDBusAbstractAdaptor
{
    Q_OBJECT
#ifndef ENABLE_DSS_SNIPE
    Q_CLASSINFO("D-Bus Interface", "com.deepin.dde.lockFront")
#else
    Q_CLASSINFO("D-Bus Interface", "org.deepin.dde.LockFront1")
#endif

public:
    explicit DBusLockFrontService(DBusLockAgent *parent);
    virtual ~DBusLockFrontService();

    inline DBusLockAgent *parent() const
    { return static_cast<DBusLockAgent *>(QObject::parent()); }

public: // PROPERTIES
    Q_PROPERTY(bool Visible READ visible)

    bool visible() const;

public Q_SLOTS:// METHODS
    void Show();
    void ShowUserList();
    void ShowAuth(bool active);
    void Suspend(bool enable);
    void Hibernate(bool enable);

Q_SIGNALS:
    void ChangKey(QString key);
    void Visible(bool visible);
};

#endif // DBUSLOCKFRONTSERVICE_H
