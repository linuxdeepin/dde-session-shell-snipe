// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeterworker.h"
#include "greeterpasswordchange.h"
#include "lockcontent.h"
#include "controlwidget.h"
#include "shutdownwidget.h"
#include "lockpasswordchangewidget.h"
#include "mfa_widget.h"
#include "userinfo.h"
#include "dconfig_helper.h"
#include "public_func.h"
#include "../gettext_test_catalog.h"
#include <QDir>
#include <QDBusVirtualObject>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QtEndian>
#include <clocale>
#include <gtest/gtest.h>
#include <libintl.h>
#include <security/pam_appl.h>
#include <sys/socket.h>
#include <unistd.h>

// Exercise the installed QLightDM client and its sequence filtering over a
// private socket. Like LightDM 1.30, this daemon never acknowledges Cancel.
class LightdmTestDaemon
{
public:
    ~LightdmTestDaemon()
    {
        // The 1.30 Qt wrapper retains its GObject/read watch on destruction.
        // Reserve retired descriptors until process exit so another fixture
        // cannot reuse them and dispatch through an old watch. Production has
        // one greeter connection for the lifetime of the process.
        static struct RetiredConnections {
            QList<int> descriptors;
            ~RetiredConnections()
            {
                for (int fd : descriptors)
                    close(fd);
            }
        } retired;
        for (int fd : sockets) {
            if (fd >= 0)
                retired.descriptors.append(fd);
        }
    }

    static QByteArray number(quint32 value)
    {
        QByteArray bytes(4, '\0');
        qToBigEndian(value, bytes.data());
        return bytes;
    }
    static QByteArray string(const QString &value)
    {
        const auto bytes = value.toUtf8();
        return number(bytes.size()) + bytes;
    }
    bool connectTo(QLightDM::Greeter *greeter)
    {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0)
            return false;
        const QByteArray oldTo = qgetenv("LIGHTDM_TO_SERVER_FD");
        const QByteArray oldFrom = qgetenv("LIGHTDM_FROM_SERVER_FD");
        const auto restore = qScopeGuard([&] {
            oldTo.isNull() ? qunsetenv("LIGHTDM_TO_SERVER_FD") : qputenv("LIGHTDM_TO_SERVER_FD", oldTo);
            oldFrom.isNull() ? qunsetenv("LIGHTDM_FROM_SERVER_FD") : qputenv("LIGHTDM_FROM_SERVER_FD", oldFrom);
        });
        qputenv("LIGHTDM_TO_SERVER_FD", QByteArray::number(sockets[0]));
        qputenv("LIGHTDM_FROM_SERVER_FD", QByteArray::number(sockets[0]));
        return sendPacket(0, string(QStringLiteral("1.30.0"))) && greeter->connectSync();
    }
    bool sendPacket(quint32 type, const QByteArray &data)
    {
        const auto packet = number(type) + number(data.size()) + data;
        return send(sockets[1], packet.constData(), packet.size(), MSG_NOSIGNAL) == packet.size();
    }
    bool prompt(quint32 sequence, const QString &account, const QList<QPair<int, QString>> &messages)
    {
        auto data = number(sequence) + string(account) + number(messages.size());
        for (const auto &message : messages)
            data += number(message.first) + string(message.second);
        return sendPacket(1, data);
    }
    bool complete(quint32 sequence, const QString &account, int result = PAM_SUCCESS)
    {
        return sendPacket(2, number(sequence) + string(account) + number(result));
    }
    int count(quint32 type)
    {
        readRequests();
        int count = 0;
        for (const auto &request : requests)
            count += request.first == type;
        return count;
    }
    QByteArray lastRequest(quint32 type)
    {
        readRequests();
        for (auto it = requests.crbegin(); it != requests.crend(); ++it) {
            if (it->first == type)
                return it->second;
        }
        return {};
    }
    QList<QByteArray> responsePackets()
    {
        readRequests();
        QList<QByteArray> packets;
        for (const auto &request : requests) {
            if (request.first == 3)
                packets.append(request.second);
        }
        return packets;
    }
    quint32 sequence()
    {
        const auto request = lastRequest(1);
        return request.size() >= 4 ? qFromBigEndian<quint32>(request.constData()) : 0;
    }

private:
    void readRequests()
    {
        char bytes[4096];
        ssize_t size;
        while ((size = recv(sockets[1], bytes, sizeof(bytes), MSG_DONTWAIT)) > 0)
            buffer.append(bytes, size);
        while (buffer.size() >= 8) {
            const auto type = qFromBigEndian<quint32>(buffer.constData());
            const auto length = qFromBigEndian<quint32>(buffer.constData() + 4);
            if (buffer.size() < length + 8)
                break;
            requests.append({type, buffer.mid(8, length)});
            buffer.remove(0, length + 8);
        }
    }
    int sockets[2] {-1, -1};
    QByteArray buffer;
    QList<QPair<quint32, QByteArray>> requests;
};

class GreeterFormTestUser : public ADDomainUser
{
public:
    explicit GreeterFormTestUser(uid_t uid) : ADDomainUser(uid) {}
    bool isUserValid() const override { return true; }
    QString path() const override { return accountPath; }
    QString accountPath;
};

class GreeterHintTestService : public QDBusVirtualObject
{
public:
    ~GreeterHintTestService() override
    {
        QDBusConnection::systemBus().unregisterObject(path);
        QDBusConnection::systemBus().unregisterService(DSS_DBUS::accountsService);
    }
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.interface() != DSS_DBUS::accountsUserInterface || message.member() != QStringLiteral("SetPasswordHint"))
            return false;
        ++calls;
        if (!fail)
            hint = message.arguments().value(0).toString();
        bus.send(fail ? message.createErrorReply(QDBusError::AccessDenied, QStringLiteral("denied")) : message.createReply());
        return true;
    }
    const QString path = QStringLiteral("/org/deepin/dde/Accounts1/UserGreeterHintTest");
    QString hint = QStringLiteral("newSecret!");
    int calls = 0;
    bool fail = false;
};

// Keep a real, valid proxy in DeepinAuthFramework so its existing-controller
// fast path can be exercised without installed authentication services.
class GreeterExistingControllerService : public QDBusVirtualObject
{
public:
    ~GreeterExistingControllerService() override
    {
        QDBusConnection::systemBus().unregisterObject(path);
        QDBusConnection::systemBus().unregisterService(DSS_DBUS::authenticateService);
    }
    QString introspect(const QString &) const override
    {
        return QStringLiteral("<interface name=\"%1\"><method name=\"Start\">"
                              "<arg type=\"i\" direction=\"in\"/><arg type=\"i\" direction=\"in\"/>"
                              "<arg type=\"i\" direction=\"out\"/></method></interface>")
            .arg(QString::fromLatin1(AuthControllerInter::staticInterfaceName()));
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.interface() != QString::fromLatin1(AuthControllerInter::staticInterfaceName()))
            return false;
        bus.send(message.createReply(QVariantList {0}));
        return true;
    }
    const QString path = QStringLiteral("/org/deepin/dde/Authenticate1/GreeterExistingControllerTest");
};

class GermanPamTestCatalog
{
public:
    ~GermanPamTestCatalog()
    {
        bindtextdomain("Linux-PAM", oldDirectory.constData());
        oldLanguage.isNull() ? qunsetenv("LANGUAGE") : qputenv("LANGUAGE", oldLanguage);
        std::setlocale(LC_MESSAGES, oldLocale.constData());
    }
    bool initialize()
    {
        if (!directory.isValid() || !QDir(directory.path()).mkpath(QStringLiteral("de/LC_MESSAGES")))
            return false;
        const QMap<QByteArray, QByteArray> messages {
            {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
            {"Current password: ", "Geben Sie das aktuelle Passwort ein: "},
            {"New password: ", "Geben Sie ein neues Passwort ein: "},
            {"Retype new password: ", "Geben Sie das neue Passwort erneut ein: "},
            {"Current %s password: ", "Geben Sie das aktuelle %s Passwort ein: "},
            {"New %s password: ", "Geben Sie ein neues %s Passwort ein: "},
            {"Retype new %s password: ", "Geben Sie das neue %s Passwort erneut ein: "}
        };
        if (!writeGettextCatalog(directory.filePath(QStringLiteral("de/LC_MESSAGES/Linux-PAM.mo")), messages))
            return false;
        bindtextdomain("Linux-PAM", directory.path().toUtf8().constData());
        qputenv("LANGUAGE", "en");
        return std::setlocale(LC_MESSAGES, "C") != nullptr;
    }
private:
    QTemporaryDir directory;
    QByteArray oldDirectory = bindtextdomain("Linux-PAM", nullptr);
    QByteArray oldLanguage = qgetenv("LANGUAGE");
    QByteArray oldLocale = std::setlocale(LC_MESSAGES, nullptr);
};

// A process-local configuration backend; never change host or remote policy.
class PasswordPolicyBackend : public Dtk::Core::DConfigBackend
{
public:
    bool isValid() const override { return true; }
    bool load(const QString &) override { return true; }
    QStringList keyList() const override { return {QStringLiteral("changePasswordForNormalUser")}; }
    QVariant value(const QString &, const QVariant &) const override { return allowed; }
    void setValue(const QString &, const QVariant &value) override { allowed = value.toBool(); }
    bool allowed = true;
};

class UT_GreeterPasswordForm : public testing::Test
{
protected:
    void SetUp() override
    {
        if (qEnvironmentVariable("DSS_TEST_PRIVATE_BUS") != QStringLiteral("1")
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
            GTEST_SKIP() << "Requires the isolated test bus; never use installed authentication services";
        model = std::make_unique<SessionBaseModel>();
        model->setAppType(AuthCommon::Login);
        auto user = std::make_shared<GreeterFormTestUser>(10042);
        user->setName(QStringLiteral("mock-domain-user"));
        user->setFullName(QStringLiteral("Mock User"));
        worker = std::make_unique<GreeterWorker>(model.get());
        model->updateCurrentUser(user);
        worker->m_account = user->name();
        auto *helper = DConfigHelper::instance();
        savedConfigs = helper->m_dConfigs;
        const auto name = getDefaultConfigFileName();
        policy.reset(Dtk::Core::DConfig::create(new PasswordPolicyBackend, name, name));
        helper->m_dConfigs.insert(helper->packageDConfigPath(name, name, {}), policy.get());
    }
    void TearDown() override
    {
        QTest::qWait(20);
        worker.reset();
        daemon.reset();
        model.reset();
        if (policy) {
            DConfigHelper::instance()->m_dConfigs = savedConfigs;
            policy.reset();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void disallowNormalUser()
    {
        model->currentUser()->m_accountType = 0;
        policy->setValue(QStringLiteral("changePasswordForNormalUser"), false);
    }
    bool connectLightdm()
    {
        daemon = std::make_unique<LightdmTestDaemon>();
        if (!daemon->connectTo(worker->m_greeter))
            return false;
        model->setVisible(true);
        worker->startGreeterAuth(model->currentUser()->name());
        return daemon->count(1) == 1 && worker->m_greeter->inAuthentication();
    }
    std::unique_ptr<LockContent> managedContent()
    {
        model->setAuthType(AuthCommon::AT_PAM);
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        auto content = std::make_unique<LockContent>();
        // Use the greeter's shared navigation controls without opening live
        // lock sockets or starting a virtual keyboard on the host.
        content->m_model = model.get();
        content->initUI();
        content->initConnections();
        QObject::connect(content.get(), &LockContent::requestChangePassword, worker.get(), &GreeterWorker::submitPasswordChange);
        QObject::connect(content.get(), &LockContent::requestCancelPasswordChange, worker.get(), &GreeterWorker::cancelPasswordChange);
        QObject::connect(content.get(), &LockContent::requestEndAuthentication, worker.get(), &GreeterWorker::endAuthentication);
        QObject::disconnect(model.get(), &SessionBaseModel::hasVirtualKBChanged, content.get(), nullptr);
        content->m_shutdownFrame->setParent(content.get());
        content->m_shutdownFrame->hide();
        content->setFullManagedLoginWidget(new QWidget(content.get()));
        content->pushPasswordFrame();
        return content;
    }
    QMap<QString, Dtk::Core::DConfig *> savedConfigs;
    std::unique_ptr<Dtk::Core::DConfig> policy;
    std::unique_ptr<SessionBaseModel> model;
    std::unique_ptr<GreeterWorker> worker;
    std::unique_ptr<LightdmTestDaemon> daemon;
};

TEST_F(UT_GreeterPasswordForm, ExpiryUsesSharedFormWithVerifiedPassword)
{
    worker->m_password = QStringLiteral("old");
    worker->m_passwordVerified = true;
    worker->changePasswd();
    EXPECT_TRUE(model->greeterPasswordChangeActive());
    EXPECT_TRUE(model->passwordChangePageVisible());
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_TRUE(worker->m_passwordChange->active());
    EXPECT_FALSE(worker->m_passwordChange->currentRequired());
    EXPECT_TRUE(worker->m_password.isEmpty());
    EXPECT_FALSE(worker->m_resetSessionTimer->isActive());
    worker->leavePasswordChange(false);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, DirectPamReplacementPromptStartsForm)
{
    worker->showPrompt(QStringLiteral("New password:"), QLightDM::Greeter::PromptTypeSecret);
    EXPECT_TRUE(model->greeterPasswordChangeActive());
    EXPECT_TRUE(worker->m_passwordChange->currentRequired());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, UncachedExpiryTransfersCurrentPromptWithReplacementBatch)
{
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredNormal, -1);
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "(current) NT password:"},
         {PAM_PROMPT_ECHO_OFF, "Enter new NT password:"},
         {PAM_PROMPT_ECHO_OFF, "Retype new NT password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 3; }, 3000));
    ASSERT_TRUE(worker->m_passwordChange->active());
    EXPECT_TRUE(worker->m_passwordChange->currentRequired());
    EXPECT_EQ(responses.count(), 0);
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    page->m_localSecurityCheck = false;
    page->m_current->setText("old");
    page->m_password->setText("newSecret!");
    page->m_confirmation->setText("newSecret!");
    page->submit();
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "old");
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    const QList<QByteArray> individual {
        LightdmTestDaemon::number(1) + LightdmTestDaemon::string("old"),
        LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!"),
        LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!")};
    const QList<QByteArray> combined {LightdmTestDaemon::number(3) + LightdmTestDaemon::string("old")
        + LightdmTestDaemon::string("newSecret!") + LightdmTestDaemon::string("newSecret!")};
    EXPECT_TRUE(daemon->responsePackets() == individual || daemon->responsePackets() == combined);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(4), 1);
}

TEST_F(UT_GreeterPasswordForm, UnknownUnansweredPromptCannotBeLostAtPasswordChangeEntry)
{
    ASSERT_TRUE(connectLightdm());
    const auto account = model->currentUser()->name();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(daemon->sequence(), account,
        {{PAM_PROMPT_ECHO_OFF, "Verification code:"},
         {PAM_PROMPT_ECHO_OFF, "New password:"},
         {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 3; }, 3000));
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    QTest::qWait(20);
    EXPECT_EQ(responses.count(), 0);
    EXPECT_EQ(daemon->count(3), 0);
    EXPECT_EQ(daemon->count(5), 1);
    EXPECT_FALSE(worker->m_passwordChange->active());
}

TEST_F(UT_GreeterPasswordForm, AnsweredOrdinaryPromptIsNotTransferredAgain)
{
    ASSERT_TRUE(connectLightdm());
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    worker->sendTokenToAuth(account, AuthCommon::AT_PAM, "old");
    EXPECT_EQ(daemon->count(3), 1);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 3; }, 3000));
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "newSecret!");
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
}

TEST_F(UT_GreeterPasswordForm, NewLightdmSequenceDiscardsEarlierUnansweredPrompts)
{
    ASSERT_TRUE(connectLightdm());
    model->setCurrentModeState(SessionBaseModel::PasswordMode);
    const auto account = model->currentUser()->name();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    ASSERT_TRUE(daemon->prompt(daemon->sequence(), account, {{PAM_PROMPT_ECHO_OFF, "Verification code:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    const auto originalSequence = daemon->sequence();
    worker->m_discardGreeterCompletion = true;
    worker->startGreeterAuth(account);
    ASSERT_GT(daemon->sequence(), originalSequence);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(daemon->sequence(), account,
        {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 3; }, 3000));
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    EXPECT_TRUE(worker->m_passwordChange->active());
    EXPECT_EQ(responses[0][0].toString(), "newSecret!");
}

TEST_F(UT_GreeterPasswordForm, CancelAndTimeoutRestoreMfaBeforePageCallbacksWithoutInitializationSignals)
{
    ASSERT_TRUE(connectLightdm());
    GreeterExistingControllerService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::authenticateService));
    ASSERT_TRUE(bus.registerVirtualObject(service.path, &service));
    auto *framework = worker->m_authFramework;
    auto *controller = new AuthControllerInter(DSS_DBUS::authenticateService, service.path, bus, framework);
    ASSERT_TRUE(controller->isValid());
    const auto account = model->currentUser()->name();
    framework->m_authenticateControllers->insert(account, controller);
    auto content = managedContent();
    model->m_authProperty.FrameworkState = AuthCommon::Available;
    const auto factors = AuthCommon::AT_Password | AuthCommon::AT_Fingerprint;
    bool restoredAtPageCallback = false;
    QObject::connect(model.get(), &SessionBaseModel::greeterPasswordChangePageChanged, model.get(), [&] {
        if (!model->greeterPasswordChangeActive())
            restoredAtPageCallback = model->getAuthProperty().MFAFlag && model->getAuthProperty().AuthType == factors;
    });
    for (int exit : {0, 1, 2}) {
        SCOPED_TRACE(exit);
        model->setVisible(true);
        worker->startGreeterAuth(account);
        model->setAuthType(factors);
        model->updateMFAFlag(true);
        worker->changePasswd();
        ASSERT_TRUE(worker->m_passwordChange->active());
        EXPECT_FALSE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(model->getAuthProperty().AuthType, AuthCommon::AT_PAM);
        const auto authentications = daemon->count(1);
        restoredAtPageCallback = false;
        if (exit == 0) {
            worker->cancelPasswordChange();
        } else {
            if (exit == 2)
                worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
            auto *timer = exit == 1 ? &worker->m_passwordChange->m_idleTimer : &worker->m_passwordChange->m_operationTimer;
            ASSERT_TRUE(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        }
        EXPECT_TRUE(restoredAtPageCallback);
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(model->getAuthProperty().AuthType, factors);
        EXPECT_NE(content->m_mfaWidget, nullptr);
        EXPECT_EQ(content->m_authWidget, content->m_mfaWidget);
        // Hide before the queued restart: restoration must be immediate and
        // cannot depend on a new controller's initialization callbacks.
        model->setVisible(false);
        QTest::qWait(30);
        EXPECT_EQ(daemon->count(1), authentications);
        ASSERT_TRUE(framework->authSessionExist(account));
        QSignalSpy initialization(framework, &DeepinAuthFramework::MFAFlagChanged);
        framework->CreateAuthController(account, factors, AuthCommon::Login);
        EXPECT_EQ(initialization.count(), 0);
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
    }
}

TEST_F(UT_GreeterPasswordForm, SuccessfulChangeRestoresMfaWithoutReentrantAuthentication)
{
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    const auto factors = AuthCommon::AT_Password | AuthCommon::AT_Fingerprint;
    model->setAuthType(factors);
    model->updateMFAFlag(true);
    worker->m_password = "old";
    worker->m_passwordVerified = true;
    worker->changePasswd();
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", {});
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    const auto authenticationRequests = daemon->count(1);
    const auto responsePackets = daemon->count(3);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(model->getAuthProperty().MFAFlag);
    EXPECT_EQ(model->getAuthProperty().AuthType, factors);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_EQ(daemon->count(1), authenticationRequests);
    EXPECT_EQ(daemon->count(3), responsePackets);
    EXPECT_EQ(daemon->count(4), 1);
}

TEST_F(UT_GreeterPasswordForm, NavigationRestoresFactorsWithoutRestartingHiddenAuthentication)
{
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    const auto factors = AuthCommon::AT_Password | AuthCommon::AT_Fingerprint;
    for (const auto mode : {SessionBaseModel::UserMode, SessionBaseModel::PowerMode}) {
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        worker->startGreeterAuth(model->currentUser()->name());
        model->setAuthType(factors);
        model->updateMFAFlag(true);
        worker->changePasswd();
        const auto authentications = daemon->count(1);
        model->setCurrentModeState(mode);
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(model->getAuthProperty().AuthType, factors);
        QTest::qWait(30);
        EXPECT_EQ(daemon->count(1), authentications);
        EXPECT_EQ(model->currentModeState(), mode);
        EXPECT_FALSE(worker->m_passwordChange->active());
    }
}

TEST_F(UT_GreeterPasswordForm, PasswordChangeSnapshotCannotOverwriteAnotherAccountsFactors)
{
    const auto factors = AuthCommon::AT_Password | AuthCommon::AT_Fingerprint;
    model->setAuthType(factors);
    model->updateMFAFlag(true);
    worker->changePasswd();
    // The new account's initialized properties must survive old-flow cleanup.
    model->m_authProperty.AuthType = AuthCommon::AT_Face;
    model->m_authProperty.MFAFlag = false;
    auto other = std::make_shared<GreeterFormTestUser>(10043);
    other->setName("other-factor-user");
    model->updateCurrentUser(other);
    EXPECT_EQ(model->getAuthProperty().AuthType, AuthCommon::AT_Face);
    EXPECT_FALSE(model->getAuthProperty().MFAFlag);
    EXPECT_FALSE(worker->m_passwordChange->active());
}

TEST_F(UT_GreeterPasswordForm, WinbindBatchBeforeSaveSendsEveryResponseInOrder)
{
    ASSERT_TRUE(connectLightdm());
    worker->m_password = QStringLiteral("old");
    worker->m_passwordVerified = true;
    worker->changePasswd();
    auto content = managedContent();
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    page->m_localSecurityCheck = false;
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "Enter new NT password:"},
         {PAM_PROMPT_ECHO_OFF, "Retype new NT password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 2 && !worker->m_passwordChange->busy(); }, 3000));
    EXPECT_EQ(responses.count(), 0);
    EXPECT_EQ(daemon->count(3), 0);
    EXPECT_TRUE(worker->m_passwordChange->active());
    QTest::keyClicks(page->m_password->lineEdit(), "newSecret!");
    QTest::keyClicks(page->m_confirmation->lineEdit(), "newSecret!");
    page->submit();
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    const auto replacement = LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!");
    const QList<QByteArray> individual {replacement, replacement};
    const QList<QByteArray> combined {LightdmTestDaemon::number(2)
        + LightdmTestDaemon::string("newSecret!") + LightdmTestDaemon::string("newSecret!")};
    // Deepin's response patch sends each answer separately; upstream LightDM
    // waits for every answer in the batch. Verify all wire responses in order.
    const auto packets = daemon->responsePackets();
    EXPECT_TRUE(packets == individual || packets == combined);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, FreshRetryExpiryNoticeDoesNotRequireAnotherSave)
{
    ASSERT_TRUE(connectLightdm());
    worker->changePasswd();
    const auto account = model->currentUser()->name();
    const auto originalSequence = daemon->sequence();
    ASSERT_TRUE(daemon->complete(originalSequence, account, PAM_AUTH_ERR));
    ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->sequence() != originalSequence; }, 3000));
    const auto sequence = daemon->sequence();
    auto content = managedContent();
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    page->m_localSecurityCheck = false;
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    QTest::keyClicks(page->m_current->lineEdit(), "old");
    QTest::keyClicks(page->m_password->lineEdit(), "newSecret!");
    QTest::keyClicks(page->m_confirmation->lineEdit(), "newSecret!");
    page->submit();
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "old");
    QSignalSpy forms(model.get(), &SessionBaseModel::lockPasswordChangeForm);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_ERROR_MSG, "You are required to change your password immediately (password expired)."},
         {PAM_ERROR_MSG, "您必须立即更改密码（管理员强制）。"},
         {PAM_PROMPT_ECHO_OFF, "New password:"},
         {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_TRUE(worker->m_passwordChange->busy());
    EXPECT_TRUE(page->m_error->toPlainText().isEmpty());
    for (const auto &form : forms)
        EXPECT_TRUE(form[3].toString().isEmpty());
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    const auto current = LightdmTestDaemon::number(1) + LightdmTestDaemon::string("old");
    const auto replacement = LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!");
    const QList<QByteArray> individual {current, replacement, replacement};
    const QList<QByteArray> combined {current, LightdmTestDaemon::number(2)
        + LightdmTestDaemon::string("newSecret!") + LightdmTestDaemon::string("newSecret!")};
    const auto packets = daemon->responsePackets();
    EXPECT_TRUE(packets == individual || packets == combined);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, PamExpiryErrorsBeforeSaveDoNotShowFailureAndBackendRejectionStillAllowsRetry)
{
    ASSERT_TRUE(connectLightdm());
    worker->m_password = QStringLiteral("old");
    worker->m_passwordVerified = true;
    worker->changePasswd();
    auto content = managedContent();
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    page->m_localSecurityCheck = false;
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy messages(worker->m_greeter, &QLightDM::Greeter::showMessage);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_ERROR_MSG, "You are required to change your password immediately (administrator enforced)."},
         {PAM_ERROR_MSG, "You are required to change your password immediately (password expired)."},
         {PAM_ERROR_MSG, "您必须立即更改密码（管理员强制）。"},
         {PAM_ERROR_MSG, "您必须立即更改密码（密码已经过期）。"},
         {PAM_PROMPT_ECHO_OFF, "New password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    EXPECT_EQ(messages.count(), 4);
    EXPECT_TRUE(page->m_current->isHidden());
    EXPECT_TRUE(page->m_error->isHidden());
    EXPECT_TRUE(page->m_error->toPlainText().isEmpty());
    EXPECT_EQ(responses.count(), 0);
    EXPECT_FALSE(worker->m_passwordChange->busy());

    QTest::keyClicks(page->m_password->lineEdit(), "newSecret!");
    QTest::keyClicks(page->m_confirmation->lineEdit(), "newSecret!");
    page->submit();
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_ERROR_MSG, "old newSecret! rejected by password policy"},
         {PAM_PROMPT_ECHO_OFF, "New password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 2 && !worker->m_passwordChange->busy(); }, 3000));
    EXPECT_FALSE(worker->m_passwordChange->busy());
    EXPECT_EQ(responses.count(), 1);
    EXPECT_FALSE(page->m_error->isHidden());
    EXPECT_FALSE(page->m_error->toPlainText().isEmpty());
    EXPECT_FALSE(page->m_error->toPlainText().contains("old"));
    EXPECT_FALSE(page->m_error->toPlainText().contains("newSecret!"));

    QTest::keyClicks(page->m_password->lineEdit(), "BetterNew!");
    QTest::keyClicks(page->m_confirmation->lineEdit(), "BetterNew!");
    EXPECT_TRUE(page->m_error->isHidden());
    page->submit();
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_EQ(responses.last()[0].toString(), "BetterNew!");
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, AccountLocaleRecognizesGermanPamAndCompletesLightdmConversation)
{
    GermanPamTestCatalog catalog;
    ASSERT_TRUE(catalog.initialize());
    model->currentUser()->m_locale = QStringLiteral("de_DE.UTF-8");
    worker->onCurrentUserChanged(model->currentUser());
    ASSERT_TRUE(connectLightdm());
    // A same-account preference update must not change the language of an
    // already running PAM conversation.
    model->currentUser()->m_locale = QStringLiteral("en_US.UTF-8");
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    worker->sendTokenToAuth(account, AuthCommon::AT_PAM, QStringLiteral("old"));
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Geben Sie ein neues Passwort ein: "}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->greeterPasswordChangeActive(); }, 3000));
    EXPECT_FALSE(worker->m_passwordChange->currentRequired());
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return daemon->lastRequest(3) == LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!");
    }, 3000));
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Geben Sie das neue Passwort erneut ein: "}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "newSecret!");
    EXPECT_TRUE(worker->m_passwordChange->active());
    EXPECT_EQ(daemon->lastRequest(3), LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!"));
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_STREQ(std::setlocale(LC_MESSAGES, nullptr), "C");
    EXPECT_EQ(qgetenv("LANGUAGE"), "en");
}

TEST_F(UT_GreeterPasswordForm, GermanUnixTokenTemplatesCompleteLightdmConversation)
{
    GermanPamTestCatalog catalog;
    ASSERT_TRUE(catalog.initialize());
    model->currentUser()->m_locale = QStringLiteral("de_DE.UTF-8");
    worker->onCurrentUserChanged(model->currentUser());
    ASSERT_TRUE(connectLightdm());
    worker->m_password = QStringLiteral("old");
    worker->m_passwordVerified = true;
    worker->changePasswd();
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "Geben Sie das aktuelle UNIX Passwort ein: "}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    ASSERT_TRUE(worker->m_passwordChange->active());
    EXPECT_EQ(responses.count(), 0);
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "old");
    ASSERT_TRUE(daemon->prompt(sequence, account,
        {{PAM_PROMPT_ECHO_OFF, "Geben Sie ein neues UNIX Passwort ein: "},
         {PAM_PROMPT_ECHO_OFF, "Geben Sie das neue UNIX Passwort erneut ein: "}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_STREQ(std::setlocale(LC_MESSAGES, nullptr), "C");
    EXPECT_EQ(qgetenv("LANGUAGE"), "en");
}

TEST_F(UT_GreeterPasswordForm, AccountLocaleSurvivesFailureRetryAndDoesNotLeakAcrossUserSwitch)
{
    GermanPamTestCatalog catalog;
    ASSERT_TRUE(catalog.initialize());
    model->currentUser()->m_locale = QStringLiteral("de_DE.UTF-8");
    worker->changePasswd();
    worker->showPrompt(QStringLiteral("Geben Sie ein neues Passwort ein: "), QLightDM::Greeter::PromptTypeSecret);
    ASSERT_TRUE(worker->m_passwordChange->active());
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    worker->handleAuthenticationComplete(false, model->currentUser()->name());
    ASSERT_TRUE(worker->m_passwordChange->active());
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    worker->showPrompt(QStringLiteral("Geben Sie das aktuelle Passwort ein: "), QLightDM::Greeter::PromptTypeSecret);
    worker->submitPasswordChange("fresh-old", "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "fresh-old");

    auto other = std::make_shared<GreeterFormTestUser>(10043);
    other->setName(QStringLiteral("other-locale-user"));
    other->m_locale = QStringLiteral("en_US.UTF-8");
    model->updateCurrentUser(other);
    EXPECT_FALSE(worker->m_passwordChange->active());
    // Simulate the fresh sequence after user switching, without a real daemon.
    worker->m_discardGreeterCompletion = false;
    worker->changePasswd();
    worker->submitPasswordChange("other-old", "newSecret!", "newSecret!", {});
    worker->showPrompt(QStringLiteral("Geben Sie ein neues Passwort ein: "), QLightDM::Greeter::PromptTypeSecret);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_EQ(responses.count(), 1);
    EXPECT_STREQ(std::setlocale(LC_MESSAGES, nullptr), "C");
    EXPECT_EQ(qgetenv("LANGUAGE"), "en");
}

TEST_F(UT_GreeterPasswordForm, FrameworkSuccessDoesNotSendTokensDuringPasswordChange)
{
    worker->changePasswd();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->onAuthStateChanged(AuthCommon::AT_All, AuthCommon::AS_Success, {});
    worker->onAuthFinished();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->active());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, DiscardedCompletionCannotStartSession)
{
    worker->changePasswd();
    worker->submitPasswordChange(QStringLiteral("old"), QStringLiteral("newSecret!"),
                                 QStringLiteral("newSecret!"), {});
    worker->leavePasswordChange(false);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->handleAuthenticationComplete(true, model->currentUser()->name());
    worker->handleAuthenticationComplete(true, model->currentUser()->name());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, AccountSwitchClearsSecretsAndForm)
{
    worker->m_password = QStringLiteral("old");
    worker->m_passwordVerified = true;
    worker->changePasswd();
    auto other = std::make_shared<GreeterFormTestUser>(10043);
    other->setName(QStringLiteral("other-mock-user"));
    model->updateCurrentUser(other);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_TRUE(worker->m_passwordChange->m_current.isEmpty());
}

TEST_F(UT_GreeterPasswordForm, LightdmFailureKeepsFormForFreshPasswordEntry)
{
    worker->changePasswd();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->handleAuthenticationComplete(false, model->currentUser()->name());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->active());
    EXPECT_TRUE(worker->m_passwordChange->currentRequired());
    EXPECT_TRUE(model->passwordChangePageVisible());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, SleepClearsFlowAndRejectsLateSuccess)
{
    worker->changePasswd();
    worker->submitPasswordChange(QStringLiteral("old"), QStringLiteral("newSecret!"),
                                QStringLiteral("newSecret!"), {});
    emit worker->m_login1Inter->PrepareForSleep(true);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_TRUE(model->isBlackMode());
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->handleAuthenticationComplete(true, model->currentUser()->name());
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_GreeterPasswordForm, CompletionFromAnotherAccountCannotStartSession)
{
    worker->changePasswd();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    worker->handleAuthenticationComplete(true, QStringLiteral("another-mock-account"));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->active());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, UnverifiedPasswordCannotHideCurrentPasswordField)
{
    worker->m_password = QStringLiteral("previously-incorrect");
    worker->m_passwordVerified = false;
    worker->changePasswd();
    EXPECT_TRUE(worker->m_passwordChange->currentRequired());
    EXPECT_TRUE(worker->m_passwordChange->m_current.isEmpty());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, DeniedNormalUserCannotEnterPasswordChange)
{
    disallowNormalUser();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    ASSERT_FALSE(model->currentUser()->allowToChangePassword());
    worker->m_password = "old";
    worker->m_passwordVerified = true;
    worker->changePasswd();
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_TRUE(worker->m_password.isEmpty());
    EXPECT_FALSE(worker->m_passwordVerified);
}

TEST_F(UT_GreeterPasswordForm, PamExpiryOverridesStaleCacheBeforeCheckingPermission)
{
    disallowNormalUser();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredNormal, -1);
    worker->showPrompt(QStringLiteral("新的密码： "), QLightDM::Greeter::PromptTypeSecret);
    EXPECT_EQ(model->currentUser()->expiredState(), User::ExpiredAlready);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, RevokedPermissionRejectsFormSubmission)
{
    worker->changePasswd();
    ASSERT_TRUE(worker->m_passwordChange->active());
    disallowNormalUser();
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_TRUE(worker->m_passwordChange->m_password.isEmpty());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, RevokedPermissionRejectsPendingPamResponse)
{
    worker->changePasswd();
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    disallowNormalUser();
    worker->m_passwordChange->prompt("New password:", true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !worker->m_passwordChange->active(); }, 3000));
    EXPECT_TRUE(worker->m_passwordChange->m_password.isEmpty());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
}

TEST_F(UT_GreeterPasswordForm, AdministratorKeepsExistingPasswordChangePermission)
{
    policy->setValue(QStringLiteral("changePasswordForNormalUser"), false);
    model->currentUser()->m_accountType = User::Administrator;
    worker->changePasswd();
    EXPECT_TRUE(worker->m_passwordChange->active());
    worker->leavePasswordChange(false);
}

TEST_F(UT_GreeterPasswordForm, CancelRestartsWithoutAcknowledgementAndRejectsOldSequence)
{
    ASSERT_TRUE(connectLightdm());
    const auto account = model->currentUser()->name();
    const auto oldSequence = daemon->sequence();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    QSignalSpy completions(worker->m_greeter, &QLightDM::Greeter::authenticationComplete);
    worker->changePasswd();
    worker->cancelPasswordChange();
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == 2; }, 3000));
    EXPECT_EQ(daemon->count(5), 1);
    EXPECT_EQ(completions.count(), 0);
    EXPECT_FALSE(worker->m_discardGreeterCompletion);
    const auto newSequence = daemon->sequence();
    EXPECT_GT(newSequence, oldSequence);

    QSignalSpy prompts(worker->m_greeter, &QLightDM::Greeter::showPrompt);
    ASSERT_TRUE(daemon->prompt(oldSequence, account, {{PAM_PROMPT_ECHO_OFF, "New password:"}}));
    ASSERT_TRUE(daemon->complete(oldSequence, account));
    ASSERT_TRUE(daemon->prompt(newSequence, account, {{PAM_PROMPT_ECHO_OFF, "Password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return prompts.count() == 1; }, 3000));
    EXPECT_EQ(prompts[0][0].toString(), "Password:");
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(completions.count(), 0);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_TRUE(worker->m_greeter->inAuthentication());

    worker->startAuthentication(account, AuthCommon::AT_PAM);
    worker->sendTokenToAuth(account, AuthCommon::AT_PAM, QStringLiteral("after-cancel"));
    EXPECT_EQ(daemon->lastRequest(3), LightdmTestDaemon::number(1) + LightdmTestDaemon::string("after-cancel"));
    ASSERT_TRUE(daemon->complete(newSequence, account));
    // startSessionSync receives only this synthetic daemon's session result.
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(4), 1);
}

TEST_F(UT_GreeterPasswordForm, IdleAndOperationTimeoutRecoverWithoutCancellationCompletion)
{
    ASSERT_TRUE(connectLightdm());
    QSignalSpy completions(worker->m_greeter, &QLightDM::Greeter::authenticationComplete);
    for (bool busy : {false, true}) {
        worker->changePasswd();
        if (busy)
            worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
        const int authentications = daemon->count(1);
        auto *timer = busy ? &worker->m_passwordChange->m_operationTimer : &worker->m_passwordChange->m_idleTimer;
        ASSERT_TRUE(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == authentications + 1; }, 3000));
        EXPECT_FALSE(worker->m_passwordChange->active());
        EXPECT_FALSE(worker->m_discardGreeterCompletion);
        EXPECT_EQ(completions.count(), 0);
    }
}

TEST_F(UT_GreeterPasswordForm, SuspendRejectsRepeatedOldSuccessAndWakeStartsNewSequence)
{
    ASSERT_TRUE(connectLightdm());
    const auto account = model->currentUser()->name();
    const auto oldSequence = daemon->sequence();
    worker->changePasswd();
    emit worker->m_login1Inter->PrepareForSleep(true);
    worker->startAuthentication(account, AuthCommon::AT_PAM);
    worker->createAuthentication(account);
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    QSignalSpy completions(worker->m_greeter, &QLightDM::Greeter::authenticationComplete);
    for (int count = 1; count <= 2; ++count) {
        ASSERT_TRUE(daemon->complete(oldSequence, account));
        ASSERT_TRUE(QTest::qWaitFor([&] { return completions.count() == count; }, 3000));
        EXPECT_EQ(finished.count(), 0);
        EXPECT_TRUE(worker->m_discardGreeterCompletion);
    }
    EXPECT_EQ(daemon->count(1), 1);
    emit worker->m_login1Inter->PrepareForSleep(false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == 2; }, 3000));
    EXPECT_GT(daemon->sequence(), oldSequence);
    EXPECT_FALSE(worker->m_discardGreeterCompletion);
    EXPECT_FALSE(model->isBlackMode());
}

TEST_F(UT_GreeterPasswordForm, HideCancelsQueuedRestartAndExplicitAuthenticationRecovers)
{
    ASSERT_TRUE(connectLightdm());
    worker->changePasswd();
    worker->cancelPasswordChange();
    model->setVisible(false);
    QTest::qWait(30);
    EXPECT_EQ(daemon->count(1), 1);
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    EXPECT_FALSE(worker->m_restartAfterCancel);
    worker->startAuthentication(model->currentUser()->name(), AuthCommon::AT_PAM);
    EXPECT_EQ(daemon->count(1), 1);
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    model->setVisible(true);
    worker->startAuthentication(model->currentUser()->name(), AuthCommon::AT_PAM);
    EXPECT_EQ(daemon->count(1), 2);
    EXPECT_FALSE(worker->m_discardGreeterCompletion);
}

TEST_F(UT_GreeterPasswordForm, AccountSwitchCancelsQueuedRestartForOldUser)
{
    ASSERT_TRUE(connectLightdm());
    worker->changePasswd();
    worker->cancelPasswordChange();
    auto other = std::make_shared<GreeterFormTestUser>(10043);
    other->setName(QStringLiteral("other-mock-user"));
    model->updateCurrentUser(other);
    QTest::qWait(30);
    EXPECT_EQ(daemon->count(1), 1);
    EXPECT_TRUE(worker->m_discardGreeterCompletion);
    EXPECT_FALSE(worker->m_restartAfterCancel);
    worker->createAuthentication(other->name());
    EXPECT_EQ(daemon->count(1), 2);
    EXPECT_EQ(worker->m_greeter->authenticationUser(), other->name());
    EXPECT_FALSE(worker->m_discardGreeterCompletion);
}

TEST_F(UT_GreeterPasswordForm, RemainingCanceledPamBatchCannotReopenPasswordForm)
{
    ASSERT_TRUE(connectLightdm());
    worker->changePasswd();
    QSignalSpy pages(model.get(), &SessionBaseModel::greeterPasswordChangePageChanged);
    ASSERT_TRUE(daemon->prompt(daemon->sequence(), model->currentUser()->name(),
                              {{PAM_PROMPT_ECHO_OFF, "OTP password:"}, {PAM_PROMPT_ECHO_OFF, "New password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == 2; }, 3000));
    EXPECT_EQ(pages.count(), 1);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_FALSE(model->greeterPasswordChangeActive());
    EXPECT_FALSE(worker->m_discardGreeterCompletion);
}

TEST_F(UT_GreeterPasswordForm, NavigationCancelsSavedResponsesAndRejectsLateLightdmSuccess)
{
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    const auto account = model->currentUser()->name();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    for (const auto mode : {SessionBaseModel::UserMode, SessionBaseModel::PowerMode}) {
        SCOPED_TRACE(static_cast<int>(mode));
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        worker->startGreeterAuth(account);
        const auto sequence = daemon->sequence();
        worker->changePasswd();
        worker->submitPasswordChange("old", "newSecret!", "newSecret!", "hint");
        ASSERT_TRUE(worker->m_passwordChange->busy());
        const auto auths = daemon->count(1);
        const auto responses = daemon->count(3);
        const auto cancels = daemon->count(5);
        if (mode == SessionBaseModel::UserMode)
            emit content->m_controlWidget->requestSwitchUser();
        else
            emit content->m_controlWidget->requestShutdown();
        EXPECT_EQ(model->currentModeState(), mode);
        EXPECT_FALSE(worker->m_passwordChange->active());
        EXPECT_FALSE(model->greeterPasswordChangeActive());
        EXPECT_TRUE(worker->m_discardGreeterCompletion);
        EXPECT_EQ(daemon->count(5), cancels + 1);
        ASSERT_TRUE(worker->m_greeter->inAuthentication());
        ASSERT_TRUE(daemon->prompt(sequence, account,
            {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
        ASSERT_TRUE(daemon->complete(sequence, account));
        // Keep the broken pre-fix startSessionSync from blocking the probe.
        if (worker->m_passwordChange->active()) {
            ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
        }
        // The client filters prompts after Cancel. Completion changes its
        // state even when it suppresses signals, proving the preceding packet
        // batch has been consumed before checking for leaked responses.
        ASSERT_TRUE(QTest::qWaitFor([&] { return !worker->m_greeter->inAuthentication(); }, 3000));
        EXPECT_EQ(daemon->count(3), responses);
        EXPECT_EQ(finished.count(), 0);
        EXPECT_EQ(daemon->count(4), 0);
        EXPECT_EQ(daemon->count(1), auths);
        EXPECT_EQ(model->currentModeState(), mode);
        EXPECT_TRUE(worker->m_passwordChange->m_current.isEmpty());
        EXPECT_TRUE(worker->m_passwordChange->m_password.isEmpty());
        EXPECT_TRUE(worker->m_passwordChange->takeHint().isEmpty());
    }
}

TEST_F(UT_GreeterPasswordForm, NavigationCancelsQueuedRecoveryUntilReturningToPasswordPage)
{
    ASSERT_TRUE(connectLightdm());
    const auto account = model->currentUser()->name();
    for (const auto mode : {SessionBaseModel::UserMode, SessionBaseModel::PowerMode,
                           SessionBaseModel::SessionMode, SessionBaseModel::SelectSameNameUserMode}) {
        SCOPED_TRACE(static_cast<int>(mode));
        worker->changePasswd();
        worker->cancelPasswordChange();
        model->setCurrentModeState(mode);
        const auto auths = daemon->count(1);
        QTest::qWait(30);
        EXPECT_EQ(daemon->count(1), auths);
        EXPECT_FALSE(worker->m_restartAfterCancel);
        EXPECT_TRUE(worker->m_discardGreeterCompletion);
        worker->createAuthentication(account);
        worker->startAuthentication(account, AuthCommon::AT_PAM);
        EXPECT_EQ(daemon->count(1), auths);
        EXPECT_TRUE(worker->m_discardGreeterCompletion);
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        worker->createAuthentication(account);
        EXPECT_EQ(daemon->count(1), auths + 1);
        EXPECT_FALSE(worker->m_discardGreeterCompletion);
    }
}

TEST_F(UT_GreeterPasswordForm, NavigationBackgroundClickRestartsOnlyAfterReturningToPasswordPage)
{
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    QObject::connect(content.get(), &LockContent::requestSwitchToUser, worker.get(), &GreeterWorker::switchToUser);
    for (const auto mode : {SessionBaseModel::UserMode, SessionBaseModel::PowerMode}) {
        SCOPED_TRACE(static_cast<int>(mode));
        worker->changePasswd();
        model->setCurrentModeState(mode);
        const auto auths = daemon->count(1);
        const auto oldSequence = daemon->sequence();
        QMouseEvent click(QEvent::MouseButtonRelease, QPointF(2, 2), QPointF(2, 2),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        content->mouseReleaseEvent(&click);
        EXPECT_EQ(model->currentModeState(), SessionBaseModel::PasswordMode);
        ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == auths + 1; }, 3000));
        EXPECT_GT(daemon->sequence(), oldSequence);
        EXPECT_FALSE(worker->m_discardGreeterCompletion);
        EXPECT_FALSE(worker->m_passwordChange->active());

        // Return, then navigate away again before the queued recovery runs.
        worker->changePasswd();
        model->setCurrentModeState(mode);
        content->mouseReleaseEvent(&click);
        model->setCurrentModeState(mode);
        QTest::qWait(30);
        EXPECT_EQ(daemon->count(1), auths + 1);
        EXPECT_TRUE(worker->m_discardGreeterCompletion);
        EXPECT_FALSE(worker->m_restartAfterCancel);
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        ASSERT_TRUE(QTest::qWaitFor([&] { return daemon->count(1) == auths + 2; }, 3000));
    }
}

TEST_F(UT_GreeterPasswordForm, PasswordlessAccountSetsPasswordWithoutInventingCurrentCredential)
{
    model->currentUser()->m_isPasswordValid = false;
    ASSERT_TRUE(connectLightdm());
    auto content = managedContent();
    const auto account = model->currentUser()->name();
    const auto sequence = daemon->sequence();
    // A stale credential must not override the account's empty password.
    worker->m_password = "stale-cache";
    worker->m_passwordVerified = true;
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "New password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->greeterPasswordChangeActive(); }, 3000));
    EXPECT_FALSE(worker->m_passwordChange->currentRequired());
    EXPECT_TRUE(worker->m_passwordChange->m_current.isEmpty());
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    EXPECT_TRUE(page->m_current->isHidden());
    EXPECT_FALSE(page->m_currentRequired);
    page->m_localSecurityCheck = false;
    page->m_password->setText("newSecret!");
    page->m_confirmation->setText("newSecret!");
    page->m_hint->setText("stale-cache");
    page->submit();
    ASSERT_TRUE(worker->m_passwordChange->busy());
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return daemon->lastRequest(3) == LightdmTestDaemon::number(1) + LightdmTestDaemon::string("newSecret!");
    }, 3000));
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(sequence, account, {{PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(3), 2);
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(sequence, account));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(daemon->count(4), 1);
}

TEST_F(UT_GreeterPasswordForm, PasswordlessRetryKeepsCurrentInputHiddenButStoredPasswordStillRequiresIt)
{
    model->currentUser()->m_isPasswordValid = false;
    worker->changePasswd();
    QSignalSpy forms(model.get(), &SessionBaseModel::lockPasswordChangeForm);
    worker->handleAuthenticationComplete(false, model->currentUser()->name());
    EXPECT_FALSE(worker->m_passwordChange->currentRequired());
    ASSERT_FALSE(forms.isEmpty());
    EXPECT_FALSE(forms.last()[0].toBool());
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", "hint");
    EXPECT_TRUE(worker->m_passwordChange->busy());
    ASSERT_FALSE(forms.isEmpty());
    EXPECT_FALSE(forms.last()[0].toBool());
    worker->leavePasswordChange(false);
    model->currentUser()->m_isPasswordValid = true;
    model->currentUser()->m_isNoPasswordLogin = true;
    worker->m_discardGreeterCompletion = false;
    worker->changePasswd();
    EXPECT_TRUE(worker->m_passwordChange->currentRequired());
    worker->submitPasswordChange({}, "newSecret!", "newSecret!", "hint");
    EXPECT_FALSE(worker->m_passwordChange->busy());
}

TEST_F(UT_GreeterPasswordForm, SuccessfulChangePersistsEmptyAndReplacementHintsFromSharedForm)
{
    ASSERT_TRUE(connectLightdm());
    GreeterHintTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(service.path, &service));
    auto user = std::dynamic_pointer_cast<GreeterFormTestUser>(model->currentUser());
    user->accountPath = service.path;
    user->m_passwordHint = service.hint;
    auto content = managedContent();
    const auto account = user->name();
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    for (const auto hint : {QString(), QStringLiteral("safe hint")}) {
        const auto calls = service.calls;
        const auto sessions = daemon->count(4);
        worker->startGreeterAuth(account);
        const auto sequence = daemon->sequence();
        worker->changePasswd();
        auto *page = content->m_passwordChangeWidget;
        ASSERT_NE(page, nullptr);
        EXPECT_TRUE(page->m_hint->text().isEmpty());
        page->m_localSecurityCheck = false;
        page->m_current->setText("old");
        page->m_password->setText("newSecret!");
        page->m_confirmation->setText("newSecret!");
        page->m_hint->setText(hint);
        page->submit();
        ASSERT_TRUE(worker->m_passwordChange->busy());
        QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
        ASSERT_TRUE(daemon->prompt(sequence, account,
            {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
        ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
        ASSERT_TRUE(daemon->complete(sequence, account));
        ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
        ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == sessions + 1; }, 3000));
        ASSERT_TRUE(QTest::qWaitFor([&] { return service.calls == calls + 1; }, 3000));
        EXPECT_EQ(service.hint, hint);
        EXPECT_EQ(daemon->count(4), sessions + 1);
    }
}

TEST_F(UT_GreeterPasswordForm, FailedAndCanceledChangesPreserveExistingHint)
{
    ASSERT_TRUE(connectLightdm());
    GreeterHintTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(service.path, &service));
    auto user = std::dynamic_pointer_cast<GreeterFormTestUser>(model->currentUser());
    user->accountPath = service.path;
    worker->changePasswd();
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    QSignalSpy forms(model.get(), &SessionBaseModel::lockPasswordChangeForm);
    ASSERT_TRUE(daemon->complete(daemon->sequence(), user->name(), PAM_AUTH_ERR));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !forms.isEmpty() && !worker->m_passwordChange->busy(); }, 3000));
    EXPECT_TRUE(worker->m_passwordChange->active());
    worker->leavePasswordChange(false);
    worker->handleAuthenticationComplete(true, user->name());
    QTest::qWait(30);
    EXPECT_EQ(service.calls, 0);
    EXPECT_EQ(service.hint, "newSecret!");
    EXPECT_EQ(daemon->count(4), 0);
}

TEST_F(UT_GreeterPasswordForm, HintWriteFailureDoesNotRepeatPasswordChangeOrSessionStart)
{
    ASSERT_TRUE(connectLightdm());
    GreeterHintTestService service;
    service.fail = true;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(service.path, &service));
    auto user = std::dynamic_pointer_cast<GreeterFormTestUser>(model->currentUser());
    user->accountPath = service.path;
    worker->changePasswd();
    worker->submitPasswordChange("old", "newSecret!", "newSecret!", {});
    QSignalSpy responses(worker->m_passwordChange, &GreeterPasswordChange::respond);
    ASSERT_TRUE(daemon->prompt(daemon->sequence(), user->name(),
        {{PAM_PROMPT_ECHO_OFF, "New password:"}, {PAM_PROMPT_ECHO_OFF, "Retype new password:"}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 2; }, 3000));
    QSignalSpy finished(model.get(), &SessionBaseModel::authFinished);
    ASSERT_TRUE(daemon->complete(daemon->sequence(), user->name()));
    ASSERT_TRUE(daemon->sendPacket(3, LightdmTestDaemon::number(0)));
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return service.calls == 1; }, 3000));
    QTest::qWait(30);
    EXPECT_FALSE(worker->m_passwordChange->active());
    EXPECT_EQ(service.hint, "newSecret!");
    EXPECT_EQ(responses.count(), 2);
    EXPECT_EQ(daemon->count(4), 1);
    EXPECT_EQ(finished.count(), 1);
}
