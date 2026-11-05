// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "authcommon.h"
#include "auth_password.h"
#include "auth_ukey.h"
#include "dbusconstant.h"
#include "deepinauthframework.h"
#include "encrypt_helper.h"
#include "lockworker.h"
#include "dlineeditex.h"
#include "mfa_widget.h"
#include "../authentication_test_service.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <gtest/gtest.h>
#include <pthread.h>
#include <security/pam_appl.h>
#include <unistd.h>

static bool hasAuthState(const QSignalSpy &status, AuthCommon::AuthState state)
{
    return std::any_of(status.cbegin(), status.cend(), [state](const QList<QVariant> &args) {
        return args.at(1).toInt() == state;
    });
}

class UT_AuthServiceRestart : public testing::Test
{
protected:
    void SetUp() override
    {
        // Never register a fake authentication service on the real system bus.
        // Run with: dbus-run-session -- sh -c
        // 'DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS <test> --gtest_filter=UT_AuthServiceRestart.*'
        if (qEnvironmentVariableIsEmpty("DBUS_SYSTEM_BUS_ADDRESS")
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Requires an isolated D-Bus session used as the system bus";
        }
        m_framework = new DeepinAuthFramework;
    }

    void TearDown() override
    {
        delete m_framework;
        QDBusConnection::disconnectFromBus("authentication-restart-test-owner");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    DeepinAuthFramework *m_framework = nullptr;
};

TEST_F(UT_AuthServiceRestart, FirstActivationDoesNotRestartAuthentication)
{
    QSignalSpy restarted(m_framework, &DeepinAuthFramework::serviceRestarted);
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, QString(), ":1.10");
    EXPECT_EQ(restarted.size(), 0);
}

TEST_F(UT_AuthServiceRestart, ServiceLossInvalidatesControllersWithoutRemoteQuit)
{
    auto *controller = new AuthControllerInter(":1.10",
                                               DSS_DBUS::authenticatePath + "/Session/old",
                                               QDBusConnection::systemBus(), m_framework);
    QPointer<AuthControllerInter> oldController(controller);
    m_framework->m_authenticateControllers->insert("test-user", controller);
    m_framework->m_authReminder.start(3000);
    QSignalSpy restarted(m_framework, &DeepinAuthFramework::serviceRestarted);

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", QString());

    EXPECT_TRUE(m_framework->m_authenticateControllers->isEmpty());
    EXPECT_FALSE(m_framework->m_authReminder.isActive());
    EXPECT_FALSE(m_framework->isDAStartupCompleted());
    EXPECT_EQ(restarted.size(), 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(oldController.isNull());

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, QString(), ":1.20");
    EXPECT_EQ(restarted.size(), 1);
    EXPECT_TRUE(m_framework->isDAStartupCompleted());
}

TEST_F(UT_AuthServiceRestart, DirectOwnerReplacementAlsoRestartsAuthentication)
{
    QSignalSpy restarted(m_framework, &DeepinAuthFramework::serviceRestarted);
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", ":1.20");
    EXPECT_EQ(restarted.size(), 1);
    EXPECT_TRUE(m_framework->isDAStartupCompleted());
}

TEST_F(UT_AuthServiceRestart, ServiceLossPreservesFallbackPam)
{
    // A cancellable waiting thread stands in for an independent PAM worker.
    ASSERT_EQ(pthread_create(&m_framework->m_PAMAuthThread, nullptr, [](void *) -> void * {
        for (;;) {
            pause();
        }
    }, nullptr), 0);
    ASSERT_TRUE(m_framework->IsUsingPamAuth());

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.10", QString());

    EXPECT_TRUE(m_framework->IsUsingPamAuth());
    EXPECT_FALSE(m_framework->isDAStartupCompleted());
}

TEST_F(UT_AuthServiceRestart, RealBusStopStartNotifiesOncePerRestart)
{
    const QString connectionName = "authentication-restart-test-owner";
    {
        auto owner = QDBusConnection::connectToBus(QDBusConnection::SystemBus, connectionName);
        ASSERT_TRUE(owner.isConnected());
        QSignalSpy restarted(m_framework, &DeepinAuthFramework::serviceRestarted);
        ASSERT_TRUE(owner.registerService(DSS_DBUS::authenticateService));
        QTest::qWait(50);
        EXPECT_EQ(restarted.size(), 0);

        for (int restart = 1; restart <= 2; ++restart) {
            ASSERT_TRUE(owner.unregisterService(DSS_DBUS::authenticateService));
            ASSERT_TRUE(QTest::qWaitFor([this] { return m_framework->m_serviceRestartPending; }));
            ASSERT_TRUE(owner.registerService(DSS_DBUS::authenticateService));
            ASSERT_TRUE(QTest::qWaitFor([&] { return restarted.size() == restart; }));
        }
        ASSERT_TRUE(owner.unregisterService(DSS_DBUS::authenticateService));
    }
    QDBusConnection::disconnectFromBus(connectionName);
}

TEST_F(UT_AuthServiceRestart, MissingControllerReportsErrorInsteadOfLeavingSubmissionPending)
{
    QSignalSpy status(m_framework, &DeepinAuthFramework::TokenSendFailed);
    m_framework->SendTokenToAuth("test-user", AuthCommon::AT_Password, "unused-test-token");

    ASSERT_EQ(status.size(), 1);
    EXPECT_EQ(status.at(0).at(0).toInt(), AuthCommon::AT_Password);
    EXPECT_TRUE(status.at(0).at(1).toString().isEmpty());
    EXPECT_FALSE(m_framework->m_authReminder.isActive());
}

TEST_F(UT_AuthServiceRestart, UserServiceResolvesInheritedDesktopSession)
{
    AuthenticationTestService service;
    ASSERT_TRUE(service.start());
    service.setSessionByPidFailure(true);
    const QByteArray oldSessionId = qgetenv("XDG_SESSION_ID");
    const auto restoreSessionId = qScopeGuard([&] {
        if (oldSessionId.isNull()) {
            qunsetenv("XDG_SESSION_ID");
        } else {
            qputenv("XDG_SESSION_ID", oldSessionId);
        }
    });
    qputenv("XDG_SESSION_ID", "desktop-test");
    SessionBaseModel model;
    LockWorker worker(&model);

    ASSERT_NE(worker.m_login1SessionSelf, nullptr);
    EXPECT_TRUE(worker.m_login1SessionSelf->isValid());
    EXPECT_EQ(worker.m_login1SessionSelf->path(), "/org/freedesktop/login1/session/test");
    EXPECT_TRUE(service.calls().contains("GetSession:desktop-test"));
}

TEST_F(UT_AuthServiceRestart, DelayedOldOwnerLossPreservesReplacementController)
{
    AuthenticationTestService service;
    ASSERT_TRUE(service.start());
    m_framework->CreateAuthController("test-user", AuthCommon::AT_Password, AuthCommon::Lock);
    QPointer<AuthControllerInter> controller = m_framework->m_authenticateControllers->value("test-user");
    ASSERT_FALSE(controller.isNull());
    EXPECT_EQ(controller->service(), service.owner());
    m_framework->sendExtraInfo("test-user", AuthCommon::AT_Password, "current-submission");

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, ":1.99999", QString());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_FALSE(controller.isNull());
    EXPECT_TRUE(m_framework->authSessionExist("test-user"));
    EXPECT_TRUE(m_framework->m_passwordExtraInfo.contains("test-user"));

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, service.owner(), QString());
    EXPECT_FALSE(m_framework->authSessionExist("test-user"));
    EXPECT_TRUE(m_framework->m_passwordExtraInfo.isEmpty());
}

class UT_TokenSendRecovery : public testing::TestWithParam<AuthCommon::AuthType>
{
protected:
    virtual AuthCommon::AuthType authType() const { return GetParam(); }

    void SetUp() override
    {
        if (qEnvironmentVariableIsEmpty("DBUS_SYSTEM_BUS_ADDRESS")
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Requires an isolated D-Bus session used as the system bus";
        }
        ASSERT_TRUE(m_service.start());
        m_model = new SessionBaseModel;
        m_worker = new LockWorker(m_model);
        m_model->currentUser()->m_name = m_account;
        m_model->updateFrameworkState(AuthCommon::Available);
        m_model->updateMFAFlag(true);
        m_worker->m_account = m_account;
        m_framework = m_worker->m_authFramework;
        m_framework->CreateAuthController(m_account, authType(), AuthCommon::Lock);
        ASSERT_TRUE(m_framework->authSessionExist(m_account));
        EncryptHelper::ref().m_symmetricKey = QString(16, 't');
        QSignalSpy startup(m_framework, &DeepinAuthFramework::AuthStateChanged);
        m_worker->startAuthentication(m_account, authType());
        ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(startup, AuthCommon::AS_Prompt); }));
    }

    void TearDown() override
    {
        delete m_worker;
        delete m_model;
    }

    void failToken()
    {
        QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
        m_service.failNextToken();
        m_worker->sendTokenToAuth(m_account, authType(), "test-token");
        ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
        for (const auto &args : status) {
            if (args.at(1).toInt() == AuthCommon::AS_Error) {
                EXPECT_TRUE(args.at(2).toString().isEmpty());
            }
        }
        EXPECT_EQ(m_service.acceptedTokens(), 0);
        EXPECT_FALSE(m_service.calls().contains(QString("End:%1").arg(authType())));
    }

    void sendDelayedTokenError()
    {
        m_service.setReplyDelay("SetToken", 200);
        m_service.failNextToken();
        m_service.clearCalls();
        m_worker->sendTokenToAuth(m_account, authType(), "cancelled-token");
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return m_service.calls().contains(QString("SetToken:%1").arg(authType()));
        }));
        m_service.setReplyDelay("SetToken", 0);
    }

    const QString m_account = "test-user";
    AuthenticationTestService m_service;
    SessionBaseModel *m_model = nullptr;
    LockWorker *m_worker = nullptr;
    DeepinAuthFramework *m_framework = nullptr;
};

TEST_P(UT_TokenSendRecovery, TransportErrorKeepsFactorActiveForRetry)
{
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.clearCalls();
    m_worker->sendTokenToAuth(m_account, GetParam(), "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({QString("SetToken:%1").arg(GetParam())}));
}

TEST_P(UT_TokenSendRecovery, TransportErrorRestoresInputWithoutAlert)
{
    AuthPassword password;
    AuthUKey ukey;
    password.setPasswordLineEditEnabled(true);
    AuthModule *widget = GetParam() == AuthCommon::AT_Password ? static_cast<AuthModule *>(&password) : &ukey;
    DLineEditEx *lineEdit = GetParam() == AuthCommon::AT_Password ? password.m_lineEdit : ukey.m_lineEdit;
    QObject::connect(m_model, &SessionBaseModel::authStateChanged, widget,
                     [widget, type = GetParam()](AuthCommon::AuthType authType, AuthCommon::AuthState state, const QString &message) {
        if (authType == type) {
            widget->setAuthState(state, message);
        }
    });
    widget->setAuthState(AuthCommon::AS_Verify, "Verifying...");
    lineEdit->lineEdit()->setReadOnly(true);
    lineEdit->setAlert(true);
    ASSERT_EQ(lineEdit->m_animation->state(), QAbstractAnimation::Running);

    ASSERT_NO_FATAL_FAILURE(failToken());

    EXPECT_EQ(widget->authState(), AuthCommon::AS_Error);
    EXPECT_FALSE(lineEdit->lineEdit()->isReadOnly());
    EXPECT_EQ(lineEdit->m_animation->state(), QAbstractAnimation::Stopped);
    EXPECT_FALSE(lineEdit->isAlert());

    m_service.sendStatus(GetParam(), AuthCommon::AS_Error, "PAM error");
    ASSERT_TRUE(QTest::qWaitFor([&] { return lineEdit->isAlert(); }));
}

TEST_P(UT_TokenSendRecovery, DaemonErrorStillEndsItsFactor)
{
    m_service.clearCalls();
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_service.sendStatus(GetParam(), AuthCommon::AS_Error, "PAM error");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    EXPECT_TRUE(m_service.calls().contains(QString("End:%1").arg(GetParam())));
}

TEST_P(UT_TokenSendRecovery, EndingAnotherFactorDoesNotDiscardCurrentTokenError)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    const auto otherFactor = GetParam() == AuthCommon::AT_Password ? AuthCommon::AT_Ukey : AuthCommon::AT_Password;
    m_worker->endAuthentication(m_account, otherFactor);
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    EXPECT_FALSE(m_service.calls().contains(QString("End:%1").arg(GetParam())));
}

TEST_P(UT_TokenSendRecovery, DelayedTokenReplyKeepsTheEventLoopResponsiveOnRetry)
{
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.setReplyDelay("SetToken", 300);
    m_service.clearCalls();
    QTimer heartbeat;
    QSignalSpy ticks(&heartbeat, &QTimer::timeout);
    heartbeat.start(10);
    QElapsedTimer elapsed;
    elapsed.start();

    m_worker->sendTokenToAuth(m_account, GetParam(), "test-token");

    EXPECT_LT(elapsed.elapsed(), 150);
    ASSERT_TRUE(QTest::qWaitFor([&] { return ticks.size() > 2; }, 150));
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({QString("SetToken:%1").arg(GetParam())}));
}

TEST_P(UT_TokenSendRecovery, TransportErrorDoesNotBlockSingleFactorUi)
{
    m_model->updateMFAFlag(false);
    m_service.setReplyDelay("End", 500);
    m_service.setReplyDelay("GetLimits", 500);
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    QElapsedTimer elapsed;
    elapsed.start();
    m_service.failNextToken();

    m_worker->sendTokenToAuth(m_account, GetParam(), "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));

    EXPECT_LT(elapsed.elapsed(), 200);
    EXPECT_FALSE(m_service.calls().contains(QString("End:%1").arg(GetParam())));
    QTimer heartbeat;
    QSignalSpy ticks(&heartbeat, &QTimer::timeout);
    heartbeat.start(10);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !ticks.isEmpty(); }, 150));
}

TEST_P(UT_TokenSendRecovery, ReplacedControllerDiscardsInitialTokenReply)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_framework->DestroyAuthController(m_account);
    m_framework->CreateAuthController(m_account, GetParam(), AuthCommon::Lock);
    EncryptHelper::ref().m_symmetricKey = QString(16, 't');
    m_worker->startAuthentication(m_account, GetParam());
    m_worker->sendTokenToAuth(m_account, GetParam(), "new-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
    EXPECT_TRUE(m_framework->authSessionExist(m_account));
}

TEST_P(UT_TokenSendRecovery, EndingFactorDiscardsInitialTokenReplyBeforeRestart)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_worker->endAuthentication(m_account, GetParam());
    m_worker->startAuthentication(m_account, GetParam());
    m_worker->sendTokenToAuth(m_account, GetParam(), "new-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    m_service.clearCalls();
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
    EXPECT_FALSE(m_service.calls().contains(QString("End:%1").arg(GetParam())));
}

TEST_P(UT_TokenSendRecovery, NewSubmissionDiscardsPreviousTokenReply)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_worker->sendTokenToAuth(m_account, GetParam(), "new-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
    EXPECT_FALSE(m_service.calls().contains(QString("End:%1").arg(GetParam())));
}

TEST_P(UT_TokenSendRecovery, EndingAllFactorsDiscardsInitialTokenReply)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_worker->endAuthentication(m_account, AuthCommon::AT_All);
    m_worker->startAuthentication(m_account, GetParam());
    m_worker->sendTokenToAuth(m_account, GetParam(), "new-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
}

TEST_P(UT_TokenSendRecovery, ServiceLossDiscardsInitialTokenReply)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, m_service.owner(), QString());
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
    EXPECT_FALSE(m_framework->authSessionExist(m_account));
}

TEST_P(UT_TokenSendRecovery, CompletedFactorDiscardsPendingTokenError)
{
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    ASSERT_NO_FATAL_FAILURE(sendDelayedTokenError());
    m_service.sendStatus(GetParam(), AuthCommon::AS_Success, "Verified");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Success); }));
    QTest::qWait(250);

    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
}

TEST_P(UT_TokenSendRecovery, InactiveSessionDoesNotRecreateAuthentication)
{
    m_model->m_visible = true;
    m_model->setCurrentContentType(SessionBaseModel::LockContent);
    m_worker->m_sessionManagerInter->setSync(true);
    m_worker->m_login1SessionSelf->setSync(true);
    m_service.setSessionActive(false);
    m_service.clearCalls();

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, m_service.owner(), QString());
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, QString(), m_service.owner());
    QTest::qWait(50);

    EXPECT_FALSE(m_service.calls().contains("Authenticate"));
    EXPECT_FALSE(m_framework->authSessionExist(m_account));
}

TEST_P(UT_TokenSendRecovery, ActiveSessionRecreatesAuthentication)
{
    m_model->m_visible = true;
    m_model->setCurrentContentType(SessionBaseModel::LockContent);
    m_worker->m_sessionManagerInter->setSync(true);
    m_worker->m_login1SessionSelf->setSync(true);
    m_service.setSessionActive(true);
    m_service.clearCalls();

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, m_service.owner(), QString());
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, QString(), m_service.owner());

    ASSERT_TRUE(QTest::qWaitFor([this] { return m_service.calls().contains("Authenticate"); }));
    EXPECT_TRUE(m_framework->authSessionExist(m_account));
}

TEST_P(UT_TokenSendRecovery, RestartStopsFallbackPamBeforeCreatingDaSession)
{
    m_model->m_visible = true;
    m_model->setCurrentContentType(SessionBaseModel::LockContent);
    m_worker->m_sessionManagerInter->setSync(true);
    m_worker->m_login1SessionSelf->setSync(true);
    m_framework->DestroyAuthController(m_account);
    m_model->updateFrameworkState(AuthCommon::Unavailable);
    ASSERT_EQ(pthread_create(&m_framework->m_PAMAuthThread, nullptr, [](void *) -> void * {
        for (;;) {
            pause();
        }
    }, nullptr), 0);
    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, m_service.owner(), QString());
    ASSERT_TRUE(m_framework->IsUsingPamAuth());
    m_service.beforeAuthCall = [this] { EXPECT_FALSE(m_framework->IsUsingPamAuth()); };

    m_framework->onAuthServiceOwnerChanged(DSS_DBUS::authenticateService, QString(), m_service.owner());

    ASSERT_TRUE(QTest::qWaitFor([this] { return m_framework->authSessionExist(m_account); }));
    EXPECT_EQ(m_model->getAuthProperty().FrameworkState, AuthCommon::Available);
    EXPECT_FALSE(m_framework->IsUsingPamAuth());
}

TEST_P(UT_TokenSendRecovery, CleanupStopsBothBackendsRegardlessOfFrameworkState)
{
    ASSERT_EQ(pthread_create(&m_framework->m_PAMAuthThread, nullptr, [](void *) -> void * {
        for (;;) {
            pause();
        }
    }, nullptr), 0);
    ASSERT_TRUE(m_framework->authSessionExist(m_account));
    ASSERT_EQ(m_model->getAuthProperty().FrameworkState, AuthCommon::Available);

    m_worker->destroyAuthentication(m_account);

    EXPECT_FALSE(m_framework->IsUsingPamAuth());
    EXPECT_FALSE(m_framework->authSessionExist(m_account));
    m_worker->destroyAuthentication(m_account);
}

TEST_P(UT_TokenSendRecovery, InactiveSessionDeviceChangeDoesNotClaimDevices)
{
    m_model->m_visible = true;
    m_model->updateMFAFlag(false);
    m_worker->m_login1SessionSelf->setSync(true);
    m_service.setSessionActive(false);
    m_service.clearCalls();

    m_framework->DeviceChanged(GetParam(), 1);
    QTest::qWait(50);

    EXPECT_FALSE(m_service.calls().contains("Authenticate"));
    EXPECT_FALSE(m_framework->authSessionExist(m_account));
}

TEST_P(UT_TokenSendRecovery, InactiveSessionResetTimerDoesNotClaimDevices)
{
    m_worker->m_login1SessionSelf->setSync(true);
    m_service.setSessionActive(false);
    m_service.clearCalls();

    m_worker->m_resetSessionTimer->start(1);
    QTest::qWait(50);

    EXPECT_FALSE(m_service.calls().contains("Authenticate"));
    EXPECT_FALSE(m_framework->authSessionExist(m_account));
}

INSTANTIATE_TEST_SUITE_P(PasswordAndPin, UT_TokenSendRecovery,
                        testing::Values(AuthCommon::AT_Password, AuthCommon::AT_Ukey));

class UT_PasswordRetryRecovery : public UT_TokenSendRecovery
{
protected:
    AuthCommon::AuthType authType() const override { return AuthCommon::AT_Password; }

    static QString extraInfoPayload(const QString &info)
    {
        return QString("{\"type\":1,\"data\":\"%1\"}").arg(info);
    }
};

TEST_F(UT_PasswordRetryRecovery, ServiceLossKeepsFallbackPamUsableWithoutReplacement)
{
    m_framework->DestroyAuthController(m_account);
    m_model->updateFrameworkState(AuthCommon::Unavailable);
    m_model->updateMFAFlag(false);
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    QSignalSpy restarted(m_framework, &DeepinAuthFramework::serviceRestarted);
    struct PamResult {
        DeepinAuthFramework *framework = nullptr;
        QByteArray token;
        int status = PAM_CONV_ERR;
        std::atomic<bool> finished{false};
    } result;
    result.framework = m_framework;
    const auto stopPam = qScopeGuard([this] { m_framework->DestroyAuthenticate(); });
    // Exercise the real PAM conversation without invoking the system PAM stack
    // or checking a real password. This worker has no dependency on DA.
    ASSERT_EQ(pthread_create(&m_framework->m_PAMAuthThread, nullptr, [](void *data) -> void * {
        auto *result = static_cast<PamResult *>(data);
        const pam_message prompt = {PAM_PROMPT_ECHO_OFF, "Password:"};
        const pam_message *messages = &prompt;
        pam_response *response = nullptr;
        result->status = DeepinAuthFramework::PAMConversation(1, &messages, &response, result->framework);
        if (response) {
            result->token = response[0].resp;
            free(response[0].resp);
            free(response);
        }
        result->framework->UpdateAuthState(result->status == PAM_SUCCESS ? AuthCommon::AS_Success : AuthCommon::AS_Failure,
                                           "Test PAM result");
        result->finished = true;
        return nullptr;
    }, &result), 0);
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Prompt); }));
    auto owner = QDBusConnection::connectToBus(QDBusConnection::SystemBus, AuthenticationTestService::connectionName());
    ASSERT_TRUE(owner.unregisterService(DSS_DBUS::authenticateService));
    ASSERT_TRUE(QTest::qWaitFor([this] { return m_framework->m_serviceRestartPending; }));
    ASSERT_TRUE(m_framework->IsUsingPamAuth());
    EXPECT_FALSE(m_framework->isDAStartupCompleted());

    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_PAM, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return result.finished.load(); }));
    EXPECT_EQ(result.status, PAM_SUCCESS);
    EXPECT_EQ(result.token, QByteArray("test-token"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Success); }));
    EXPECT_TRUE(restarted.isEmpty());
    EXPECT_EQ(m_model->getAuthProperty().FrameworkState, AuthCommon::Unavailable);
    EXPECT_FALSE(QDBusConnection::systemBus().interface()->isServiceRegistered(DSS_DBUS::authenticateService).value());
}

TEST_F(UT_PasswordRetryRecovery, SendsCurrentExtraInfoBeforeRetryToken)
{
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "previous-submission");
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.clearCalls();
    m_service.requirePasswordExtraInfo(extraInfoPayload("current-submission"));
    m_service.setReplyDelay("SetExtraInfo", 200);
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "superseded");
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "current-submission");
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.calls().contains("SetExtraInfo:1"); }));
    QTest::qWait(50);
    EXPECT_FALSE(m_service.calls().contains("SetToken:1"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({"SetExtraInfo:1", "SetToken:1"}));
    EXPECT_EQ(m_service.acceptedPasswordExtraInfo(), extraInfoPayload("current-submission"));
}

TEST_F(UT_PasswordRetryRecovery, SendsExplicitlyEmptyExtraInfo)
{
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.requirePasswordExtraInfo(extraInfoPayload(QString()));
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, QString());
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.acceptedPasswordExtraInfo(), extraInfoPayload(QString()));
}

TEST_F(UT_PasswordRetryRecovery, FailedExtraInfoDoesNotSendTokenAndCanBeRetried)
{
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.clearCalls();
    m_service.failNextExtraInfo();
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "current-submission");
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Error); }));
    EXPECT_FALSE(m_service.calls().contains("SetToken:1"));
    EXPECT_FALSE(m_service.calls().contains("End:1"));
    m_service.requirePasswordExtraInfo(extraInfoPayload("new-submission"));
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "new-submission");
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.acceptedPasswordExtraInfo(), extraInfoPayload("new-submission"));
}

TEST_F(UT_PasswordRetryRecovery, TransportRetryKeepsAlreadyDeliveredExtensionData)
{
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "extension-result");
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.requirePasswordExtraInfo(extraInfoPayload("extension-result"));
    m_service.clearCalls();
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({"SetToken:1"}));
    EXPECT_EQ(m_service.acceptedPasswordExtraInfo(), extraInfoPayload("extension-result"));
}

TEST_F(UT_PasswordRetryRecovery, EndingFactorClearsUnsubmittedExtensionData)
{
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "abandoned-submission");
    m_worker->endAuthentication(m_account, AuthCommon::AT_Password);
    m_worker->startAuthentication(m_account, AuthCommon::AT_Password);
    m_service.clearCalls();
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");

    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    EXPECT_EQ(m_service.calls(), QStringList({"SetToken:1"}));
    EXPECT_TRUE(m_service.acceptedPasswordExtraInfo().isNull());
}

TEST_F(UT_PasswordRetryRecovery, EndingFactorDiscardsPendingExtraInfoReply)
{
    m_service.setReplyDelay("SetExtraInfo", 100);
    m_service.clearCalls();
    m_worker->sendExtraInfo(m_account, AuthCommon::AT_Password, "cancelled-submission");
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.calls().contains("SetExtraInfo:1"); }));
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);

    m_worker->endAuthentication(m_account, AuthCommon::AT_Password);
    QTest::qWait(200);

    EXPECT_FALSE(m_service.calls().contains("SetToken:1"));
    EXPECT_EQ(m_service.acceptedTokens(), 0);
    EXPECT_TRUE(m_framework->m_passwordExtraInfo.isEmpty());
    EXPECT_FALSE(hasAuthState(status, AuthCommon::AS_Error));
}

TEST_F(UT_PasswordRetryRecovery, MfaRetryDoesNotReplayInformationalStartupPrompts)
{
    MFAWidget widget;
    widget.setModel(m_model);
    widget.setAuthType(AuthCommon::AT_Password);
    QObject::connect(&widget, &MFAWidget::sendTokenToAuth, m_worker, &LockWorker::sendTokenToAuth);
    AuthPassword *password = widget.m_passwordAuth;
    ASSERT_NE(password, nullptr);
    m_service.setPasswordStartupInfo({"Password policy information:", "More information before the challenge"});
    m_worker->endAuthentication(m_account, AuthCommon::AT_Password);
    QSignalSpy rawStatus(m_framework->m_authenticateControllers->value(m_account), &AuthControllerInter::Status);
    m_worker->startAuthentication(m_account, AuthCommon::AT_Password);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return std::any_of(rawStatus.cbegin(), rawStatus.cend(), [](const QList<QVariant> &args) {
            return args.at(1).toInt() == AuthCommon::AS_Prompt && args.at(2).toString() == "Password: ";
        });
    }));
    QStringList prompts;
    for (const auto &args : rawStatus) {
        if (args.at(1).toInt() == AuthCommon::AS_Prompt) {
            prompts << args.at(2).toString();
        }
    }
    EXPECT_EQ(prompts, QStringList({"Password policy information:", "More information before the challenge", "Password: "}));
    m_service.clearCalls();
    ASSERT_NO_FATAL_FAILURE(failToken());
    m_service.clearCalls();
    password->m_lineEdit->setText("test-token");
    m_service.setReplyDelay("SetToken", 200);
    QSignalSpy submitted(&widget, &MFAWidget::sendTokenToAuth);

    password->requestAuthenticate();
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QTest::qWait(250);

    EXPECT_EQ(password->authState(), AuthCommon::AS_Verify);
    EXPECT_TRUE(password->m_lineEdit->lineEdit()->isReadOnly());
    EXPECT_EQ(password->m_lineEdit->m_animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(m_service.calls(), QStringList({"SetToken:1"}));
    QTest::keyClick(password->m_lineEdit->lineEdit(), Qt::Key_Return);
    EXPECT_EQ(submitted.size(), 1);

    m_service.sendStatus(AuthCommon::AT_Password, AuthCommon::AS_Error, "Verification interrupted");
    ASSERT_TRUE(QTest::qWaitFor([&] { return !password->m_lineEdit->lineEdit()->isReadOnly(); }));
    EXPECT_EQ(password->m_lineEdit->m_animation->state(), QAbstractAnimation::Stopped);
}

TEST_F(UT_PasswordRetryRecovery, RetryDoesNotSuppressOtherFactorsOrAdditionalChallenges)
{
    ASSERT_NO_FATAL_FAILURE(failToken());
    AuthPassword password;
    password.setPasswordLineEditEnabled(true);
    password.setAuthState(AuthCommon::AS_Verify, "Verifying...");
    QObject::connect(m_model, &SessionBaseModel::authStateChanged, &password,
                     [&password](AuthCommon::AuthType type, AuthCommon::AuthState state, const QString &text) {
        if (type == AuthCommon::AT_Password) {
            password.setAuthState(state, text);
        }
    });
    m_worker->sendTokenToAuth(m_account, AuthCommon::AT_Password, "test-token");
    ASSERT_TRUE(QTest::qWaitFor([&] { return m_service.acceptedTokens() == 1; }));
    QSignalSpy status(m_model, &SessionBaseModel::authStateChanged);
    m_service.sendStatus(AuthCommon::AT_Ukey, AuthCommon::AS_Prompt, "PIN:");
    ASSERT_TRUE(QTest::qWaitFor([&] { return hasAuthState(status, AuthCommon::AS_Prompt); }));
    EXPECT_EQ(status.last().at(0).toInt(), AuthCommon::AT_Ukey);
    EXPECT_TRUE(password.m_lineEdit->lineEdit()->isReadOnly());

    m_service.sendStatus(AuthCommon::AT_Password, AuthCommon::AS_Prompt, "Additional password challenge");
    ASSERT_TRUE(QTest::qWaitFor([&] { return !password.m_lineEdit->lineEdit()->isReadOnly(); }));
    EXPECT_EQ(password.m_lineEdit->m_animation->state(), QAbstractAnimation::Stopped);
}
