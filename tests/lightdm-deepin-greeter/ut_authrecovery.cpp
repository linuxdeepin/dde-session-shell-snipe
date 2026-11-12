// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeterworker.h"
#include "auth_password.h"
#include "dlineeditex.h"
#ifndef ENABLE_DSS_SNIPE
#include "encrypt_helper.h"
#endif
#include "sfa_widget.h"
#include "../authentication_test_service.h"

#include <QDataStream>
#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QtEndian>
#include <algorithm>
#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>

static bool hasAuthState(const QSignalSpy &status, AuthCommon::AuthState state)
{
    return std::any_of(status.cbegin(), status.cend(), [state](const QList<QVariant> &args) {
        return args.at(1).toInt() == state;
    });
}

// Use LightDM's real greeter protocol over test pipes, without a display manager.
class LightdmTestPeer
{
public:
    bool start()
    {
        if (pipe(m_toServer) != 0 || pipe(m_fromServer) != 0) {
            return false;
        }
        if (fcntl(m_toServer[0], F_SETFL, O_NONBLOCK) < 0) {
            return false;
        }
        qputenv("LIGHTDM_TO_SERVER_FD", QByteArray::number(m_toServer[1]));
        qputenv("LIGHTDM_FROM_SERVER_FD", QByteArray::number(m_fromServer[0]));
        QByteArray connected;
        QDataStream stream(&connected, QIODevice::WriteOnly);
        stream << quint32(0) << quint32(8) << quint32(4);
        stream.writeRawData("test", 4);
        return write(m_fromServer[1], connected.constData(), connected.size()) == connected.size();
    }

    ~LightdmTestPeer()
    {
        for (int fd : {m_toServer[0], m_toServer[1], m_fromServer[0], m_fromServer[1]}) {
            if (fd >= 0) {
                close(fd);
            }
        }
        if (m_oldToServer.isNull()) {
            qunsetenv("LIGHTDM_TO_SERVER_FD");
        } else {
            qputenv("LIGHTDM_TO_SERVER_FD", m_oldToServer);
        }
        if (m_oldFromServer.isNull()) {
            qunsetenv("LIGHTDM_FROM_SERVER_FD");
        } else {
            qputenv("LIGHTDM_FROM_SERVER_FD", m_oldFromServer);
        }
    }

    void drain(AuthenticationTestService &service)
    {
        QMutexLocker locker(&m_mutex);
        char bytes[1024];
        ssize_t size;
        while ((size = read(m_toServer[0], bytes, sizeof(bytes))) > 0) {
            m_buffer.append(bytes, size);
        }
        while (m_buffer.size() >= 8) {
            const quint32 id = qFromBigEndian<quint32>(m_buffer.constData());
            const quint32 length = qFromBigEndian<quint32>(m_buffer.constData() + 4);
            if (m_buffer.size() < 8 + length) {
                break;
            }
            if (id == 1) { // GREETER_MESSAGE_AUTHENTICATE
                service.recordLightdmAuthentication();
            }
            m_buffer.remove(0, 8 + length);
        }
    }

private:
    int m_toServer[2] = {-1, -1};
    int m_fromServer[2] = {-1, -1};
    QByteArray m_oldToServer = qgetenv("LIGHTDM_TO_SERVER_FD");
    QByteArray m_oldFromServer = qgetenv("LIGHTDM_FROM_SERVER_FD");
    QByteArray m_buffer;
    QMutex m_mutex;
};

class UT_GreeterAuthRecovery : public testing::Test
{
protected:
    void SetUp() override
    {
        if (qEnvironmentVariableIsEmpty("DBUS_SYSTEM_BUS_ADDRESS")
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Requires an isolated D-Bus session used as the system bus";
        }
        ASSERT_TRUE(m_lightdm.start());
        // Drain earlier LightDM writes before recording each DA setup call,
        // so the ordering assertion does not depend on thread scheduling.
        m_service.beforeAuthCall = [this] { m_lightdm.drain(m_service); };
        ASSERT_TRUE(m_service.start());
        m_model = new SessionBaseModel;
        m_worker = new GreeterWorker(m_model);
#ifdef QT_DEBUG
        // GreeterWorker only connects to LightDM automatically in non-Debug builds.
        ASSERT_TRUE(m_worker->m_greeter->connectSync());
#endif
        ASSERT_NE(m_worker->m_login1SessionSelf, nullptr);
        m_worker->m_login1SessionSelf->setSync(true);
        m_model->currentUser()->m_name = "test-user";
        m_model->m_visible = true;
    }

    void TearDown() override
    {
        delete m_worker;
        delete m_model;
    }

    void checkRestartOrdering(bool alreadyAuthenticating)
    {
        if (alreadyAuthenticating) {
            m_worker->m_greeter->authenticate("test-user");
            ASSERT_TRUE(m_worker->m_greeter->inAuthentication());
        }
        m_lightdm.drain(m_service);
        m_service.clearCalls();
        // Widgets can start a factor while CreateAuthController is emitting
        // properties; this must still start exactly one LightDM conversation.
        QObject::connect(m_worker->m_authFramework, &DeepinAuthFramework::FactorsInfoChanged,
                         m_worker, [this] { m_worker->startAuthentication("test-user", AuthCommon::AT_Password); });
        m_worker->m_authFramework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", ":1.20");
        ASSERT_TRUE(QTest::qWaitFor([this] {
            m_lightdm.drain(m_service);
            return m_service.calls().contains("LightDM.Authenticate");
        }));
        const auto calls = m_service.calls();
        ASSERT_TRUE(calls.contains("PrivilegesEnable"));
        EXPECT_LT(calls.indexOf("Authenticate"), calls.indexOf("PrivilegesEnable"));
        EXPECT_LT(calls.indexOf("PrivilegesEnable"), calls.indexOf("LightDM.Authenticate"));
        EXPECT_EQ(calls.count("LightDM.Authenticate"), 1);
        EXPECT_FALSE(m_worker->m_restartGreeterAuth);
    }

    LightdmTestPeer m_lightdm;
    AuthenticationTestService m_service;
    SessionBaseModel *m_model = nullptr;
    GreeterWorker *m_worker = nullptr;
};

TEST_F(UT_GreeterAuthRecovery, RestoresPrivilegesBeforeReplacingExistingConversation)
{
    checkRestartOrdering(true);
}

TEST_F(UT_GreeterAuthRecovery, RestoresPrivilegesBeforeStartingIdleConversation)
{
    checkRestartOrdering(false);
}

TEST(UT_GreeterAuthRecoveryWithoutLogin1, RestartSkipsAuthentication)
{
    if (qEnvironmentVariableIsEmpty("DBUS_SYSTEM_BUS_ADDRESS")
        || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
        GTEST_SKIP() << "Requires an isolated D-Bus session used as the system bus";
    }
    LightdmTestPeer lightdm;
    ASSERT_TRUE(lightdm.start());
    AuthenticationTestService service;
    ASSERT_TRUE(service.start(false));
    SessionBaseModel model;
    GreeterWorker worker(&model);
#ifdef QT_DEBUG
    ASSERT_TRUE(worker.m_greeter->connectSync());
#endif
    ASSERT_FALSE(worker.m_login1Inter->isValid());
    ASSERT_EQ(worker.m_login1SessionSelf, nullptr);
    model.currentUser()->m_name = "test-user";
    model.m_visible = true;
    worker.m_password = "old-password";
    worker.m_retryAuth = true;
    worker.m_resetSessionTimer->start();
    model.updateAuthState(AuthCommon::AT_All, AuthCommon::AS_Success, "Old success");
    lightdm.drain(service);
    service.clearCalls();
    QSignalSpy status(&model, &SessionBaseModel::authStateChanged);

    worker.m_authFramework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", ":1.20");

    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Cancel); }));
    lightdm.drain(service);
    EXPECT_FALSE(worker.m_resetSessionTimer->isActive());
    EXPECT_TRUE(worker.m_password.isEmpty());
    EXPECT_FALSE(worker.m_retryAuth);
    EXPECT_EQ(model.getAuthProperty().FrameworkState, AuthCommon::Available);
    EXPECT_FALSE(service.calls().contains("Authenticate"));
    EXPECT_FALSE(service.calls().contains("LightDM.Authenticate"));
}

TEST_F(UT_GreeterAuthRecovery, RestartDoesNotReuseSuccessWhenLightdmPromptsAgain)
{
    m_model->setAppType(AuthCommon::Login);
    m_model->updateFrameworkState(AuthCommon::Available);
    m_model->updateMFAFlag(false);
    SFAWidget widget;
    // Keep a successful password widget alive, as the real SFA page does.
    widget.m_model = m_model;
    widget.m_user = m_model->currentUser();
    widget.m_passwordAuth = new AuthPassword(&widget);
    widget.m_passwordAuth->setAuthState(AuthCommon::AS_Success, "Old success");
    m_model->updateAuthState(AuthCommon::AT_All, AuthCommon::AS_Success, "Old success");
    m_worker->m_greeter->authenticate("test-user");
    QSignalSpy finished(&widget, &SFAWidget::authFinished);
    QObject::connect(m_model, &SessionBaseModel::lightdmPamStartedChanged,
                     &widget, &SFAWidget::onLightdmPamStartChanged);
    QObject::connect(&widget, &SFAWidget::authFinished, m_worker, &GreeterWorker::onAuthFinished);
    QObject::connect(m_worker->m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_worker, [this] {
        // A prompt can arrive before the replacement DA has verified anything.
        m_worker->showPrompt("Password:", QLightDM::Greeter::PromptTypeSecret);
    });

    m_worker->m_authFramework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", ":1.20");

    ASSERT_TRUE(QTest::qWaitFor([this] { return m_model->isLightdmPamStarted(); }));
    EXPECT_TRUE(finished.isEmpty());
    EXPECT_NE(m_model->getAuthResult().authState, AuthCommon::AS_Success);
}

TEST_F(UT_GreeterAuthRecovery, PinRetryKeepsCurrentFactorActive)
{
    m_worker->createAuthentication("test-user");
#ifndef ENABLE_DSS_SNIPE
    EncryptHelper::ref().m_symmetricKey = QString(16, 't');
#endif
    m_worker->startAuthentication("test-user", AuthCommon::AT_Ukey);
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_service.failNextToken();
    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    EXPECT_FALSE(m_service.calls().contains("End:16"));
    m_service.clearCalls();

    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({AuthenticationTestService::tokenCall(AuthCommon::AT_Ukey)}));
}

TEST_F(UT_GreeterAuthRecovery, PinErrorAndRetryKeepTheEventLoopResponsive)
{
    m_worker->createAuthentication("test-user");
#ifndef ENABLE_DSS_SNIPE
    EncryptHelper::ref().m_symmetricKey = QString(16, 't');
#endif
    m_worker->startAuthentication("test-user", AuthCommon::AT_Ukey);
    m_service.setReplyDelay("End", 500);
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    QElapsedTimer elapsed;
    elapsed.start();
    m_service.failNextToken();

    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    EXPECT_LT(elapsed.elapsed(), 200);
    EXPECT_FALSE(m_service.calls().contains("End:16"));
    m_service.setReplyDelay(AuthenticationTestService::tokenMethod(), 300);
    m_service.clearCalls();
    QTimer heartbeat;
    QSignalSpy ticks(&heartbeat, &QTimer::timeout);
    heartbeat.start(10);
    elapsed.restart();

    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "test-token");
    EXPECT_LT(elapsed.elapsed(), 150);
    ASSERT_TRUE(QTest::qWaitFor([&] { return ticks.size() > 2; }, 150));
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({AuthenticationTestService::tokenCall(AuthCommon::AT_Ukey)}));
}

TEST_F(UT_GreeterAuthRecovery, SfaRetryDoesNotReplayInformationalStartupPrompts)
{
    m_worker->createAuthentication("test-user");
    m_model->updateMFAFlag(false);
#ifndef ENABLE_DSS_SNIPE
    EncryptHelper::ref().m_symmetricKey = QString(16, 't');
#endif
    m_service.setPasswordStartupInfo({"PAM startup information:", "More information before the password challenge"});
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_worker->startAuthentication("test-user", AuthCommon::AT_Password);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return std::any_of(status.cbegin(), status.cend(), [](const QList<QVariant> &args) {
            return args.at(1).toInt() == AuthCommon::AS_Prompt && args.at(2).toString() == "Password: ";
        });
    }));
    EXPECT_GE(status.size(), 4);
    status.clear();
    m_service.failNextToken();
    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Password, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    m_service.clearCalls();

    SFAWidget widget;
    widget.setModel(m_model);
    widget.setAuthType(AuthCommon::AT_Password);
    QObject::connect(&widget, &SFAWidget::sendTokenToAuth, m_worker, &GreeterWorker::sendTokenToAuth);
    AuthPassword *password = widget.m_passwordAuth;
    ASSERT_NE(password, nullptr);
    password->m_lineEdit->setText("test-token");
    m_service.setReplyDelay(AuthenticationTestService::tokenMethod(), 200);
    QSignalSpy submitted(&widget, &SFAWidget::sendTokenToAuth);

    password->requestAuthenticate();
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);
    EXPECT_EQ(m_service.calls(), QStringList({AuthenticationTestService::tokenCall(AuthCommon::AT_Password)}));
    EXPECT_EQ(password->authState(), AuthCommon::AS_Verify);
    EXPECT_TRUE(password->m_lineEdit->lineEdit()->isReadOnly());
    EXPECT_EQ(password->m_lineEdit->m_animation->state(), QAbstractAnimation::Running);
    QTest::keyClick(password->m_lineEdit->lineEdit(), Qt::Key_Return);
    EXPECT_EQ(submitted.size(), 1);

    m_service.sendStatus(AuthCommon::AT_Password, AuthCommon::AS_Success, "Verified");
    ASSERT_TRUE(QTest::qWaitFor([&] { return password->authState() == AuthCommon::AS_Success; }));
    EXPECT_EQ(password->m_lineEdit->m_animation->state(), QAbstractAnimation::Stopped);
}

TEST_F(UT_GreeterAuthRecovery, EndingPinDiscardsInitialTokenReplyBeforeRestart)
{
    m_worker->createAuthentication("test-user");
#ifndef ENABLE_DSS_SNIPE
    EncryptHelper::ref().m_symmetricKey = QString(16, 't');
#endif
    m_worker->startAuthentication("test-user", AuthCommon::AT_Ukey);
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_service.clearCalls();
    m_service.failNextToken();
    m_service.setReplyDelay(AuthenticationTestService::tokenMethod(), 200);
    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "cancelled-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.calls().contains(AuthenticationTestService::tokenCall(AuthCommon::AT_Ukey)); }));
    m_worker->endAuthentication("test-user", AuthCommon::AT_Ukey);
    m_worker->startAuthentication("test-user", AuthCommon::AT_Ukey);
    m_service.setReplyDelay(AuthenticationTestService::tokenMethod(), 0);
    m_worker->sendTokenToAuth("test-user", AuthCommon::AT_Ukey, "new-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
}
