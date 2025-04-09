// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "socketHelper.h"

namespace dss {
namespace module {

static QMap<QString, void (SocketHelper::*)(QLocalSocket *, const QByteArray &)> s_FunMap = {
    {"getPublicEncrypt", &SocketHelper::getPublicEncrypt},
    {"sendAuth", &SocketHelper::sendAuth},
    {"getCurrentUserAuthResult", &SocketHelper::getCurrentUserAuthResult},
    {"getPrivateEncrypt", &SocketHelper::getPrivateEncrypt}
};

SocketHelper::SocketHelper(QString ServiceName, QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_serverName(ServiceName)
    , m_lastData("")
{
    initService();
}

SocketHelper::~SocketHelper()
{
    if (m_server) {
        delete m_server;
    }
}

void SocketHelper::initService()
{
    if (m_server) {
        return;
    }

    m_server = new QLocalServer(this);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(newConnectionHandler()));
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);

    m_server->removeServer(m_serverName);
    bool islisten = m_server->listen(m_serverName);
    if (!islisten) {
        islisten = m_server->listen(m_serverName);
    }
}

void SocketHelper::newConnectionHandler()
{
    QLocalSocket *socket = m_server->nextPendingConnection();
    connect(socket, SIGNAL(readyRead()), this, SLOT(readyReadHandler()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(disconnectedHandler()));
    m_clients.append(socket);
}

void SocketHelper::disconnectedHandler()
{
    QLocalSocket *socket = static_cast<QLocalSocket *>(sender());
    if (socket) {
        m_clients.removeAll(socket);
        socket->deleteLater();
    }
}

void SocketHelper::readyReadHandler()
{
    QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());
    if (!socket) {
        return;
    }

    QByteArray allData = socket->readAll();
    allData = m_lastData + allData;
    QList<QByteArray> dataArray = allData.split('\n');
    m_lastData = dataArray.last();
    for (const QByteArray &data : dataArray) {
        int keyIndex = data.indexOf(':');
        if (keyIndex != -1) {
            QString key = data.left(keyIndex);
            QByteArray value = data.mid(keyIndex + 1);
            if (s_FunMap.contains(key)) {
                (this->*s_FunMap.value(key))(socket, value);
            }
        }
    }

}

void SocketHelper::getPublicEncrypt(QLocalSocket *socket, const QByteArray &data)
{
    Q_EMIT createPublicEncrypt();
    socket->write("\nreceive:" + data + "\n");
}

void SocketHelper::sendAuth(QLocalSocket *socket, const QByteArray &data)
{
    QList<QByteArray> value = data.split(',');
    if (data.count() > 1) {
        QString user = QString(value[0]);
        QString pwd = QString(value[1]);
        Q_EMIT startAuth(user, pwd);
    }
    socket->write("\nreceive:" + data + "\n");
}

void SocketHelper::getCurrentUserAuthResult(QLocalSocket *socket, const QByteArray &data)
{
    Q_UNUSED(data);
    socket->write("\nreceive:" + data + "\n");
}

void SocketHelper::sendDataToClient(const QByteArray &data)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); it++) {
        (*it)->write(data);
    }
}

void SocketHelper::getPrivateEncrypt(QLocalSocket *socket, const QByteArray &data)
{
    Q_EMIT createPrivateEncrypt();
    socket->write("\nreceive:" + data + "\n");
}
}
}
