// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUTHENTICATION_TEST_SERVICE_H
#define AUTHENTICATION_TEST_SERVICE_H

#include "dbusconstant.h"
#include "deepinauthframework.h"

#include <QDBusUnixFileDescriptor>
#include <QDBusVirtualObject>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QtEndian>
#include <cerrno>
#include <functional>
#include <unistd.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(AuthCommon::AuthType)
#endif

// Run only on a private bus. A separate thread lets the real synchronous DA
// proxies complete without depending on the test/UI thread's event loop.
class AuthenticationTestService : public QDBusVirtualObject
{
public:
    bool start(bool login1Available = true)
    {
        if (qEnvironmentVariableIsEmpty("DBUS_SYSTEM_BUS_ADDRESS")
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            return false;
        }
        registerMFAInfoListMetaType();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        qRegisterMetaType<AuthCommon::AuthType>("AuthType");
        qRegisterMetaType<AuthCommon::AuthState>("AuthState");
#endif
        moveToThread(&m_thread);
        m_thread.start();
        bool registered = false;
        QMetaObject::invokeMethod(this, [this, &registered, login1Available] {
            auto bus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, connectionName());
            registered = bus.registerVirtualObject("/", this, QDBusConnection::SubPath)
                && bus.registerService(DSS_DBUS::authenticateService)
                && bus.registerService(DSS_DBUS::accountsService)
                && bus.registerService(DSS_DBUS::sessionManagerService)
                && (!login1Available || bus.registerService("org.freedesktop.login1"));
        }, Qt::BlockingQueuedConnection);
        return registered;
    }

    ~AuthenticationTestService() override
    {
        if (!m_thread.isRunning()) {
            return;
        }
        QThread *callerThread = QThread::currentThread();
        QMetaObject::invokeMethod(this, [this, callerThread] {
            auto bus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, connectionName());
            bus.unregisterObject("/", QDBusConnection::UnregisterTree);
            bus.unregisterService(DSS_DBUS::authenticateService);
            bus.unregisterService(DSS_DBUS::accountsService);
            bus.unregisterService(DSS_DBUS::sessionManagerService);
            bus.unregisterService("org.freedesktop.login1");
            QDBusConnection::disconnectFromBus(connectionName());
            moveToThread(callerThread);
        }, Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral("<interface name=\"%1\"/><interface name=\"%2\">"
                              "<signal name=\"Status\"><arg type=\"i\"/><arg type=\"i\"/>"
                              "<arg type=\"s\"/></signal></interface>"
                              "<interface name=\"org.freedesktop.login1.Manager\"/>"
                              "<interface name=\"org.freedesktop.login1.Session\"/>")
            .arg(AuthInter::staticInterfaceName(), AuthControllerInter::staticInterfaceName());
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        const QString method = message.member();
        if ((method == "Authenticate" || method == "PrivilegesEnable") && beforeAuthCall) {
            beforeAuthCall();
        }
        QMutexLocker locker(&m_mutex);
        QList<QVariant> reply;
        if (method == "Get" || method == "GetAll") {
            const QVariantMap properties = {
                {"FrameworkState", int(AuthCommon::Available)}, {"SupportedFlags", 17},
                {"SupportEncrypts", QString("{}")}, {"IsMFA", true}, {"IsFuzzyMFA", false},
                {"PINLen", 0}, {"Prompt", QString("Password:")},
                {"FactorsInfo", QVariant::fromValue(MFAInfoList())}, {"Active", m_sessionActive},
                {"Locked", true}, {"UserList", QStringList()}, {"IsTerminalLocked", false}
            };
            if (method == "Get") {
                const QString property = message.arguments().at(1).toString();
                if (!properties.contains(property)) {
                    return bus.send(message.createErrorReply(QDBusError::UnknownProperty, property));
                }
                reply << QVariant::fromValue(QDBusVariant(properties.value(property)));
            } else {
                reply << properties;
            }
        } else if (method == "GetSessionByPID") {
            if (m_failSessionByPid) {
                return bus.send(message.createErrorReply("org.freedesktop.login1.NoSessionForPID", "No session for user service"));
            }
            reply << QVariant::fromValue(QDBusObjectPath("/org/freedesktop/login1/session/test"));
        } else if (method == "GetSession") {
            m_calls << "GetSession:" + message.arguments().first().toString();
            reply << QVariant::fromValue(QDBusObjectPath("/org/freedesktop/login1/session/test"));
        } else if (method == "GetLimits") {
            reply << QString("{}");
        } else if (method == "FindUserByName") {
            reply << QString("/org/deepin/dde/Accounts1/User1000");
        } else if (method == "Authenticate") {
            m_calls << method;
            reply << sessionPath();
        } else if (method == "PrivilegesEnable") {
            m_calls << method;
            reply << true;
        } else if (method == "EncryptKey" || method == "SetSymmetricKey") {
            m_encryptionCalls << method;
#ifdef ENABLE_DSS_SNIPE
            return sendReply(bus, message.createErrorReply(QDBusError::UnknownMethod, "Encryption is unavailable"), m_replyDelays.value(method));
#else
            if (method == "EncryptKey") {
                reply << 0 << QVariant::fromValue(ArrayInt()) << QString();
            }
#endif
        } else if (method == "SetToken" || method == "SetTokenByFD" || method == "SetExtraInfo" || method == "Start" || method == "End") {
            const int factor = message.arguments().first().toInt();
            m_calls << QString("%1:%2").arg(method).arg(factor);
            if (method == "SetToken") {
                m_tokenPayload = message.arguments().at(1).toByteArray();
            } else if (method == "SetTokenByFD") {
                if (!m_supportsTokenByFD) {
                    return sendReply(bus, message.createErrorReply(QDBusError::UnknownMethod, method), m_replyDelays.value(method));
                }
                if (!readPasswordPayload(message.arguments().at(1).value<QDBusUnixFileDescriptor>(), m_fdToken)) {
                    return sendReply(bus, message.createErrorReply(QDBusError::InvalidArgs, "Invalid password frame"), m_replyDelays.value(method));
                }
            }
            if (method == "End") {
                m_startedFactors &= ~factor;
                if (factor & AuthCommon::AT_Password) {
                    m_passwordExtraInfo = QString();
                }
                sendFactorStatus(bus, factor, AuthCommon::AS_Ended);
                reply << 0;
            } else if (method == "Start") {
                m_startedFactors |= factor;
                if (factor & AuthCommon::AT_Password) {
                    m_passwordExtraInfo = QString();
                }
                sendFactorStatus(bus, factor, AuthCommon::AS_Started);
                for (const QString &info : m_passwordStartupInfo) {
                    sendFactorStatus(bus, factor & AuthCommon::AT_Password, AuthCommon::AS_Prompt, info);
                }
                const int promptDelay = m_replyDelays.value("Prompt");
                if (promptDelay > 0) {
                    QTimer::singleShot(promptDelay, this, [this, bus, factor] {
                        sendFactorStatus(bus, factor, AuthCommon::AS_Prompt);
                    });
                } else {
                    sendFactorStatus(bus, factor, AuthCommon::AS_Prompt);
                }
                reply << 0;
            } else if (method == "SetExtraInfo") {
                if (m_failExtraInfo) {
                    m_failExtraInfo = false;
                    return sendReply(bus, message.createErrorReply(QDBusError::Failed, "Injected extra information transport failure"), m_replyDelays.value(method));
                }
                if (factor == AuthCommon::AT_Password && (m_startedFactors & factor)) {
                    m_passwordExtraInfo = message.arguments().at(1).toString();
                }
            } else if (m_failToken) {
                m_failToken = false;
                return sendReply(bus, message.createErrorReply(QDBusError::Failed, "Injected token transport failure"), m_replyDelays.value(method));
            } else if ((m_startedFactors & factor)
                       && (factor != AuthCommon::AT_Password || m_requiredExtraInfo.isNull()
                           || m_passwordExtraInfo == m_requiredExtraInfo)) {
                ++m_acceptedTokens;
                if (factor == AuthCommon::AT_Password) {
                    m_acceptedPasswordExtraInfo = m_passwordExtraInfo;
                }
            }
        }
        return sendReply(bus, message.createReply(reply), m_replyDelays.value(method));
    }

    QStringList calls() const
    {
        QMutexLocker locker(&m_mutex);
        return m_calls;
    }

    void clearCalls()
    {
        QMutexLocker locker(&m_mutex);
        m_calls.clear();
    }

    void recordLightdmAuthentication()
    {
        QMutexLocker locker(&m_mutex);
        m_calls << "LightDM.Authenticate";
    }

    void failNextToken()
    {
        QMutexLocker locker(&m_mutex);
        m_failToken = true;
    }

    void failNextExtraInfo()
    {
        QMutexLocker locker(&m_mutex);
        m_failExtraInfo = true;
    }

    void requirePasswordExtraInfo(const QString &info)
    {
        QMutexLocker locker(&m_mutex);
        m_requiredExtraInfo = info;
    }

    QString acceptedPasswordExtraInfo() const
    {
        QMutexLocker locker(&m_mutex);
        return m_acceptedPasswordExtraInfo;
    }

    static QString tokenMethod()
    {
#ifdef ENABLE_DSS_SNIPE
        return QStringLiteral("SetTokenByFD");
#else
        return QStringLiteral("SetToken");
#endif
    }

    static QString tokenCall(int factor)
    {
        return QString("%1:%2").arg(tokenMethod()).arg(factor);
    }

    static QString extraInfoCall(int factor)
    {
        return QStringLiteral("SetExtraInfo:%1").arg(factor);
    }

    QStringList encryptionCalls() const
    {
        QMutexLocker locker(&m_mutex);
        return m_encryptionCalls;
    }

    void setSupportsTokenByFD(bool supported)
    {
        QMutexLocker locker(&m_mutex);
        m_supportsTokenByFD = supported;
    }

    QByteArray fdToken() const
    {
        QMutexLocker locker(&m_mutex);
        return m_fdToken;
    }

    QByteArray tokenPayload() const
    {
        QMutexLocker locker(&m_mutex);
        return m_tokenPayload;
    }

    void sendStatus(AuthCommon::AuthType factor, AuthCommon::AuthState state, const QString &text = QString())
    {
        QMetaObject::invokeMethod(this, [this, factor, state, text] {
            auto bus = QDBusConnection::connectToBus(QDBusConnection::SystemBus, connectionName());
            QDBusMessage signal = QDBusMessage::createSignal(sessionPath(), AuthControllerInter::staticInterfaceName(), "Status");
            signal << int(factor) << int(state) << text;
            bus.send(signal);
        }, Qt::BlockingQueuedConnection);
    }

    void setReplyDelay(const QString &method, int delay)
    {
        QMutexLocker locker(&m_mutex);
        m_replyDelays[method] = delay;
    }

    void setPasswordStartupInfo(const QStringList &messages)
    {
        QMutexLocker locker(&m_mutex);
        m_passwordStartupInfo = messages;
    }

    void setSessionActive(bool active)
    {
        QMutexLocker locker(&m_mutex);
        m_sessionActive = active;
    }

    void setSessionByPidFailure(bool fail)
    {
        QMutexLocker locker(&m_mutex);
        m_failSessionByPid = fail;
    }

    int acceptedTokens() const
    {
        QMutexLocker locker(&m_mutex);
        return m_acceptedTokens;
    }

    static QString sessionPath() { return DSS_DBUS::authenticatePath + "/Session/test"; }
    QString owner() const
    {
        return QDBusConnection::connectToBus(QDBusConnection::SystemBus, connectionName()).baseService();
    }
    std::function<void()> beforeAuthCall;

private:
    bool sendReply(const QDBusConnection &bus, const QDBusMessage &reply, int delay)
    {
        if (delay > 0) {
            QTimer::singleShot(delay, this, [bus, reply] { bus.send(reply); });
            return true;
        }
        return bus.send(reply);
    }

    void sendFactorStatus(const QDBusConnection &bus, int factors, AuthCommon::AuthState state, const QString &text = QString("Password: "))
    {
        for (auto factor : {AuthCommon::AT_Password, AuthCommon::AT_Ukey}) {
            if (factors & factor) {
                QDBusMessage signal = QDBusMessage::createSignal(sessionPath(), AuthControllerInter::staticInterfaceName(), "Status");
                signal << int(factor) << int(state) << text;
                bus.send(signal);
            }
        }
    }

    static bool readPasswordPayload(const QDBusUnixFileDescriptor &descriptor, QByteArray &password)
    {
        if (!descriptor.isValid()) {
            return false;
        }
        QByteArray payload;
        char buffer[1024];
        for (;;) {
            const ssize_t ret = ::read(descriptor.fileDescriptor(), buffer, sizeof(buffer));
            if (ret < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            if (ret == 0) {
                break;
            }
            payload.append(buffer, static_cast<int>(ret));
        }
        if (payload.size() < static_cast<int>(sizeof(quint32))) {
            return false;
        }
        const quint32 length = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(payload.constData()));
        if (length > 1024 || payload.size() != static_cast<int>(sizeof(quint32) + length)) {
            return false;
        }
        password = payload.mid(sizeof(quint32), static_cast<int>(length));
        return true;
    }

    static QString connectionName() { return "authentication-recovery-test-service"; }
    QThread m_thread;
    mutable QMutex m_mutex;
    QStringList m_calls;
    QStringList m_encryptionCalls;
    QMap<QString, int> m_replyDelays;
    QStringList m_passwordStartupInfo;
    QString m_passwordExtraInfo;
    QString m_requiredExtraInfo;
    QString m_acceptedPasswordExtraInfo;
    int m_startedFactors = 0;
    int m_acceptedTokens = 0;
    bool m_failToken = false;
    bool m_failExtraInfo = false;
    bool m_sessionActive = true;
    QByteArray m_fdToken;
#ifdef ENABLE_DSS_SNIPE
    bool m_supportsTokenByFD = true;
#else
    bool m_supportsTokenByFD = false;
#endif
    QByteArray m_tokenPayload;
    bool m_failSessionByPid = false;
};

#endif // AUTHENTICATION_TEST_SERVICE_H
