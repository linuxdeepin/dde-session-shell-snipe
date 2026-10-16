// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbusconstant.h"
#include "lockcontent.h"
#include "sessionbasemodel.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>
#include <gtest/gtest.h>

class SystemInfoTestService : public QDBusVirtualObject
{
public:
    mutable std::atomic<int> introspectionCalls{0};
    std::atomic<int> propertyCalls{0};
    QDBusMessage pendingRequest;

    QString introspect(const QString &) const override
    {
        ++introspectionCalls;
        return QStringLiteral("<interface name=\"org.freedesktop.DBus.Properties\">"
                              "<method name=\"Get\"><arg type=\"s\" direction=\"in\"/>"
                              "<arg type=\"s\" direction=\"in\"/><arg type=\"v\" direction=\"out\"/>"
                              "</method></interface>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override
    {
        if (message.interface() != QLatin1String("org.freedesktop.DBus.Properties")
            || message.member() != QLatin1String("Get")
            || message.arguments() != QVariantList{DSS_DBUS::systemInfoService, QStringLiteral("CPUHardware")}) {
            return false;
        }
        // Hold the reply until init() has returned. A synchronous property read
        // would block startup; an interface probe increments introspectionCalls.
        message.setDelayedReply(true);
        pendingRequest = message;
        ++propertyCalls;
        return true;
    }
};

class UT_SystemInfo : public testing::Test
{
protected:
    void SetUp() override
    {
        if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS").isEmpty()
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Run with tests/dde-lock/run-background-tests.sh";
        }
        service = new SystemInfoTestService;
        service->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, service, &QObject::deleteLater);
        thread.start();
        bool registered = false;
        QMetaObject::invokeMethod(service, [&] {
            auto bus = QDBusConnection::connectToBus(
                QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS")), connectionName);
            registered = bus.registerService(DSS_DBUS::systemInfoService)
                && bus.registerVirtualObject(DSS_DBUS::systemInfoPath, service);
        }, Qt::BlockingQueuedConnection);
        ASSERT_TRUE(registered);
    }

    void TearDown() override
    {
        if (service) {
            QMetaObject::invokeMethod(service, [&] {
                auto bus = QDBusConnection(connectionName);
                bus.unregisterObject(DSS_DBUS::systemInfoPath);
                bus.unregisterService(DSS_DBUS::systemInfoService);
                QDBusConnection::disconnectFromBus(connectionName);
            }, Qt::BlockingQueuedConnection);
            thread.quit();
            thread.wait();
        }
    }

    const QString connectionName = QStringLiteral("systeminfo-regression-test");
    QThread thread;
    SystemInfoTestService *service = nullptr;
};

TEST_F(UT_SystemInfo, InitializationDoesNotProbeInterfaceOrWaitForCpuHardware)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    SessionBaseModel model;
    model.setAppType(AuthCommon::Lock);
    auto user = std::make_shared<User>();
    // init() creates a local socket based on the username; keep it test-specific.
    user->m_name = QStringLiteral("systeminfo-test-") + directory.path().section('/', -1);
    model.updateCurrentUser(user);
    LockContent content;
    content.init(&model);

    EXPECT_EQ(service->introspectionCalls, 0);
    EXPECT_FALSE(content.m_isPANGUCpu);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->propertyCalls == 1; }, 1000));
    QMetaObject::invokeMethod(service, [&] {
        QDBusConnection(connectionName).send(service->pendingRequest.createReply(
            QVariantList{QVariant::fromValue(QDBusVariant(QStringLiteral("PANGU test CPU")))}));
    }, Qt::BlockingQueuedConnection);
    ASSERT_TRUE(QTest::qWaitFor([&] { return content.m_isPANGUCpu; }, 1000));
}
