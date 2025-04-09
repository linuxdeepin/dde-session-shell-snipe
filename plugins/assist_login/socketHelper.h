// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SOCKETHELPER_H
#define SOCKETHELPER_H

#include "QObject"
#include "QtNetwork/QLocalSocket"
#include "QtNetwork/QLocalServer"

namespace dss {

namespace module {

class SocketHelper: public QObject
{
    Q_OBJECT
public:
    explicit SocketHelper(QString ServiceName, QObject *parent = Q_NULLPTR);
    ~SocketHelper();

    void getPublicEncrypt(QLocalSocket *socket, const QByteArray &data);
    void getPrivateEncrypt(QLocalSocket *socket, const QByteArray &data);
    void sendAuth(QLocalSocket *socket, const QByteArray &data);
    void getCurrentUserAuthResult(QLocalSocket *socket, const QByteArray &data);
    void sendDataToClient(const QByteArray &data);

Q_SIGNALS:
    void createPublicEncrypt();
    void createPrivateEncrypt();
    void getAuthResult();
    void startAuth(const QString &user, const QString &passwd);

private Q_SLOTS:
    void newConnectionHandler();
    void readyReadHandler();
    void disconnectedHandler();

private:
    void initService();

    QLocalServer *m_server;
    QList<QLocalSocket *> m_clients;
    QString m_serverName;
    QByteArray m_lastData;
};

}
}

#endif //SOCKETHELPER_H
