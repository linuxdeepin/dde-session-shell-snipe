// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userservice.h"
#include "waypointmodel.h"

#include <QDBusReply>
#include <QDebug>

const QString kAccountService = "com.deepin.daemon.Accounts";
const QString kAccountPath = "/com/deepin/daemon/Accounts";
const QString kUserInterface = "com.deepin.daemon.Accounts.User";
const QString kPropertyInterface = "org.freedesktop.DBus.Properties";
const QString kEnableFlag = "PatternEnabled";
const QString kStateFlag = "PatternState";

using namespace gestureLogin;

UserService::UserService(const QString &name, QObject *parent)
    : QObject(parent)
    , m_userInter(nullptr)
    , m_currentName(name)
{
    init();
}

UserService *UserService::instance()
{
    static UserService s("");
    return &s;
}

void UserService::setUserName(const QString &userName)
{
    // 不论设置用户名多少，都重新做初始化
    m_currentName = userName;
    init();
}

bool UserService::gestureEnabled()
{
    auto rst = false;
    if (m_userInter && m_userInter->isValid()) {
        rst = m_userInter->property("PatternEnabled").toBool();
    }

    return rst;
}

int UserService::gestureFlags()
{
    auto rst = -1;
    if (m_userInter && m_userInter->isValid()) {
        rst = m_userInter->property("PatternState").toInt();
    }

    return rst;
}

QString UserService::localeName()
{
    QString rst = "";
    if (m_userInter && m_userInter->isValid()) {
        rst = m_userInter->property("Locale").toString();
    }

    return rst;
}

void UserService::enroll(const QString &data)
{
    if (data.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "enroll data empty";
        return;
    }

    if (m_userInter && m_userInter->isValid()) {
        m_userInter->call("SetPattern", data);
        qDebug() << Q_FUNC_INFO << "enrolled" << m_userInter->path();
    }
}

void UserService::onPropertiesChanged(const QDBusMessage& msg)
{
    QList<QVariant> arguments = msg.arguments();
    if (3 != arguments.count())
        return;

    QString interfaceName = msg.arguments().at(0).toString();

    if (interfaceName != kUserInterface)
        return;

    QVariantMap changedProps = qdbus_cast<QVariantMap>(arguments.at(1).value<QDBusArgument>());
    QStringList keys = changedProps.keys();
    if (changedProps.keys().contains(kStateFlag)) {
        WayPointModel::instance()->setGestureState(changedProps.value(kStateFlag).toInt());
    }
}

void UserService::init()
{
    if (m_userInter) {
        m_userInter->disconnect();
        delete m_userInter;
        m_userInter = nullptr;
    }

    if (!m_userInterpath.isEmpty()) {
        QDBusConnection::systemBus().disconnect(kAccountService, m_userInterpath, kPropertyInterface,  "PropertiesChanged", "sa{sv}as", this, SLOT(onPropertiesChanged(QDBusMessage)));
    }

    static QDBusInterface accountInter(kAccountService, kAccountPath, kAccountService, QDBusConnection::systemBus(), this);
    QDBusReply<QString> reply = accountInter.call("FindUserByName", m_currentName);
    if (reply.isValid()) {
        m_userInterpath = reply;
    }

    if (m_userInterpath.isEmpty()) {
        return;
    }

    m_userInter = new QDBusInterface(kAccountService, m_userInterpath, kUserInterface, QDBusConnection::systemBus(), this);
    if (m_userInter->isValid()) {
        bool connected = QDBusConnection::systemBus().connect(kAccountService, m_userInterpath, kPropertyInterface,  "PropertiesChanged", "sa{sv}as", this, SLOT(onPropertiesChanged(QDBusMessage)));
        qDebug() << Q_FUNC_INFO << "property change handle" << connected;

        auto model = WayPointModel::instance();
        model->setGestureEnabled(gestureEnabled());
        model->setGestureState(gestureFlags());
        model->setLocaleName(localeName());
    } else {
        qWarning() << Q_FUNC_INFO << "user interface invalid" << m_userInterpath;
    }

}
