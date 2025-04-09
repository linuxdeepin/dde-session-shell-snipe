/*
 * This file was generated by qdbusxml2cpp-fix version 0.8
 * Command line was: qdbusxml2cpp-fix -p /home/zyz/works/work/v25/dde-session-shell-snipe/src/global_util/dbus/sessionmanager1interface /home/zyz/works/work/v25/dde-session-shell-snipe/xml/snipe/org.deepin.dde.SessionManager1.xml
 *
 * qdbusxml2cpp-fix is Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#include "/home/zyz/works/work/v25/dde-session-shell-snipe/src/global_util/dbus/sessionmanager1interface.h"

DCORE_USE_NAMESPACE
/*
 * Implementation of interface class __OrgDeepinDdeSessionManager1Interface
 */

class __OrgDeepinDdeSessionManager1InterfacePrivate
{
public:
   __OrgDeepinDdeSessionManager1InterfacePrivate() = default;

    // begin member variables
    QString CurrentUid;
    bool Locked;
    int Stage;

public:
    QMap<QString, QDBusPendingCallWatcher *> m_processingCalls;
    QMap<QString, QList<QVariant>> m_waittingCalls;
};

__OrgDeepinDdeSessionManager1Interface::__OrgDeepinDdeSessionManager1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent)
    : DDBusExtendedAbstractInterface(service, path, staticInterfaceName(), connection, parent)
    , d_ptr(new __OrgDeepinDdeSessionManager1InterfacePrivate)
{
    connect(this, &__OrgDeepinDdeSessionManager1Interface::propertyChanged, this, &__OrgDeepinDdeSessionManager1Interface::onPropertyChanged);

}

__OrgDeepinDdeSessionManager1Interface::~__OrgDeepinDdeSessionManager1Interface()
{
    qDeleteAll(d_ptr->m_processingCalls.values());
    delete d_ptr;
}

void __OrgDeepinDdeSessionManager1Interface::onPropertyChanged(const QString &propName, const QVariant &value)
{
    if (propName == QStringLiteral("CurrentUid"))
    {
        const QString &CurrentUid = qvariant_cast<QString>(value);
        if (d_ptr->CurrentUid != CurrentUid)
        {
            d_ptr->CurrentUid = CurrentUid;
            Q_EMIT CurrentUidChanged(d_ptr->CurrentUid);
        }
        return;
    }

    if (propName == QStringLiteral("Locked"))
    {
        const bool &Locked = qvariant_cast<bool>(value);
        if (d_ptr->Locked != Locked)
        {
            d_ptr->Locked = Locked;
            Q_EMIT LockedChanged(d_ptr->Locked);
        }
        return;
    }

    if (propName == QStringLiteral("Stage"))
    {
        const int &Stage = qvariant_cast<int>(value);
        if (d_ptr->Stage != Stage)
        {
            d_ptr->Stage = Stage;
            Q_EMIT StageChanged(d_ptr->Stage);
        }
        return;
    }

    qWarning() << "property not handle: " << propName;
    return;
}

QString __OrgDeepinDdeSessionManager1Interface::currentUid()
{
    return qvariant_cast<QString>(internalPropGet("CurrentUid", &d_ptr->CurrentUid));
}

bool __OrgDeepinDdeSessionManager1Interface::locked()
{
    return qvariant_cast<bool>(internalPropGet("Locked", &d_ptr->Locked));
}

int __OrgDeepinDdeSessionManager1Interface::stage()
{
    return qvariant_cast<int>(internalPropGet("Stage", &d_ptr->Stage));
}

void __OrgDeepinDdeSessionManager1Interface::CallQueued(const QString &callName, const QList<QVariant> &args)
{
    if (d_ptr->m_waittingCalls.contains(callName))
    {
        d_ptr->m_waittingCalls[callName] = args;
        return;
    }
    if (d_ptr->m_processingCalls.contains(callName))
    {
        d_ptr->m_waittingCalls.insert(callName, args);
    } else {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(asyncCallWithArgumentList(callName, args));
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &__OrgDeepinDdeSessionManager1Interface::onPendingCallFinished);
        d_ptr->m_processingCalls.insert(callName, watcher);
    }
}

void __OrgDeepinDdeSessionManager1Interface::onPendingCallFinished(QDBusPendingCallWatcher *w)
{
    w->deleteLater();
    const auto callName = d_ptr->m_processingCalls.key(w);
    Q_ASSERT(!callName.isEmpty());
    if (callName.isEmpty())
        return;
    d_ptr->m_processingCalls.remove(callName);
    if (!d_ptr->m_waittingCalls.contains(callName))
        return;
    const auto args = d_ptr->m_waittingCalls.take(callName);
    CallQueued(callName, args);
}
