// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "securityloaderhelper.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <unistd.h>

Q_LOGGING_CATEGORY(secLoader, "org.deepin.dde.lock.securityloader")

const QString SecurityLoaderHelper::DEFAULT_CONFIG_PATH = ":/files/permission-interfaces/auth.json";

SecurityLoaderHelper::SecurityLoaderHelper(QObject *parent)
    : QObject(parent)
{
}

SecurityLoaderHelper::~SecurityLoaderHelper() {}

SecurityLoaderHelper &SecurityLoaderHelper::instance()
{
    static SecurityLoaderHelper instance;
    return instance;
}

void SecurityLoaderHelper::doSecurityLoader(int fd1, int fd2)
{
    if (fd1 < 0 || fd2 < 0) {
        qCWarning(secLoader) << "Not loaded by loader, skipping handshake";
        return;
    }

    qCInfo(secLoader) << "Detected loader injection: fd1=" << fd1 << "fd2=" << fd2;

    loadConfig();
    // appendCurrentUserAccountsUserDest();
    if (!performHandshake(fd1, fd2)) {
        qCWarning(secLoader) << "Security loader handshake failed";
    }
}

void SecurityLoaderHelper::loadConfig(const QString &configPath)
{
    QString path = configPath.isEmpty() ? DEFAULT_CONFIG_PATH : configPath;

    qCInfo(secLoader) << "Loading permission config from:" << path;

    m_destList = QJsonArray();
    parseJsonFile(path);

    qCInfo(secLoader) << "Loaded" << m_destList.size() << "D-Bus interfaces to authorize";
}

void SecurityLoaderHelper::parseJsonFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(secLoader) << "Cannot open file:" << filePath;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qCWarning(secLoader) << "JSON parse error in" << filePath << ":" << error.errorString();
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("DestList") || !root["DestList"].isArray()) {
        qCWarning(secLoader) << "Invalid config format in" << filePath << ": missing DestList";
        return;
    }

    for (const auto &item : root["DestList"].toArray()) {
        appendDest(item.toObject());
    }
}

void SecurityLoaderHelper::appendDest(const QJsonObject &dest)
{
    const QString dbusName = dest["DbusName"].toString();
    const QString dbusPath = dest["DbusPath"].toString();
    const QString dbusInterface = dest["DbusInterface"].toString();

    if (dbusName.isEmpty() || dbusPath.isEmpty() || dbusInterface.isEmpty()) {
        qCWarning(secLoader) << "Skip invalid D-Bus destination:" << dest;
        return;
    }

    for (const auto &existing : m_destList) {
        const QJsonObject current = existing.toObject();
        if (current["DbusName"] == dbusName &&
            current["DbusPath"] == dbusPath &&
            current["DbusInterface"] == dbusInterface) {
            return;
        }
    }

    m_destList.append(dest);
    qCInfo(secLoader) << "  Added:" << dbusName << dbusPath << dbusInterface;
}

void SecurityLoaderHelper::appendCurrentUserAccountsUserDest()
{
    const QString userPath = currentUserAccountsPath();
    if (userPath.isEmpty()) {
        qCWarning(secLoader) << "Cannot resolve current user's Accounts.User path";
        return;
    }

    QJsonObject dest;
#ifdef ENABLE_DSS_SNIPE
    dest["DbusName"] = QStringLiteral("org.deepin.dde.Accounts1");
    dest["DbusPath"] = userPath;
    dest["DbusInterface"] = QStringLiteral("org.deepin.dde.Accounts1.User");
#else
    dest["DbusName"] = QStringLiteral("com.deepin.daemon.Accounts");
    dest["DbusPath"] = userPath;
    dest["DbusInterface"] = QStringLiteral("com.deepin.daemon.Accounts.User");
#endif
    appendDest(dest);
}

QString SecurityLoaderHelper::currentUserAccountsPath() const
{
    QDBusConnection systemBus = QDBusConnection::systemBus();
    if (!systemBus.isConnected()) {
        qCWarning(secLoader) << "Cannot connect to system bus when resolving current user path";
        return {};
    }

#ifdef ENABLE_DSS_SNIPE
    QDBusInterface accountsInterface(QStringLiteral("org.deepin.dde.Accounts1"),
                                     QStringLiteral("/org/deepin/dde/Accounts1"),
                                     QStringLiteral("org.deepin.dde.Accounts1"),
                                     systemBus);
#else
    QDBusInterface accountsInterface(QStringLiteral("com.deepin.daemon.Accounts"),
                                     QStringLiteral("/com/deepin/daemon/Accounts"),
                                     QStringLiteral("com.deepin.daemon.Accounts"),
                                     systemBus);
#endif
    if (!accountsInterface.isValid()) {
        qCWarning(secLoader) << "Accounts interface invalid when resolving current user path:"
                             << accountsInterface.lastError().message();
        return {};
    }

    QDBusReply<QString> reply = accountsInterface.call(QStringLiteral("FindUserById"), QString::number(getuid()));
    if (!reply.isValid()) {
        qCWarning(secLoader) << "FindUserById failed when resolving current user path:"
                             << reply.error().message();
        return {};
    }

    return reply.value();
}

bool SecurityLoaderHelper::performHandshake(int fd1, int fd2)
{
    if (m_destList.isEmpty()) {
        qCInfo(secLoader) << "No D-Bus interfaces loaded, skipping handshake";
        return true;
    }

    qCInfo(secLoader) << "Performing loader handshake...";

    QDBusConnection systemBus = QDBusConnection::systemBus();
    if (!systemBus.isConnected()) {
        qCWarning(secLoader) << "Cannot connect to system bus";
        return false;
    }
    systemBus.interface()->isServiceRegistered("org.freedesktop.DBus");
    QString uniqueName = systemBus.baseService();
    qCInfo(secLoader) << "System Bus UniqueName:" << uniqueName;

    QJsonObject request;
    request["UniqueName"] = uniqueName;
    request["DestList"] = m_destList;

    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    qCInfo(secLoader) << "Sending request with" << m_destList.size() << "interfaces";

    QFile fd1File;
    if (!fd1File.open(fd1, QIODevice::WriteOnly)) {
        qCWarning(secLoader) << "Cannot open fd1 for writing";
        return false;
    }
    fd1File.write(jsonData);
    fd1File.close();
    qCInfo(secLoader) << "Sent authorization request to loader";

    QFile fd2File;
    if (!fd2File.open(fd2, QIODevice::ReadOnly)) {
        qCWarning(secLoader) << "Cannot open fd2 for reading";
        return false;
    }

    QByteArray response = fd2File.readAll();
    fd2File.close();

    QJsonParseError parseError;
    QJsonDocument responseDoc = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(secLoader) << "Invalid JSON response from loader:" << parseError.errorString();
        return false;
    }

    QJsonObject result = responseDoc.object();
    if (result["Result"].toBool()) {
        qCInfo(secLoader) << "Loader handshake completed successfully";
        return true;
    } else {
        qCWarning(secLoader) << "Loader authorization response:" << result["Message"].toString();
        return false;
    }
}
