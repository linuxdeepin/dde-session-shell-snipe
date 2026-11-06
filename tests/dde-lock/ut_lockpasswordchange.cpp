// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockpasswordchange.h"
#include "lockworker.h"
#include "lockframe.h"
#include "auth_single.h"
#include "auth_password.h"
#include "dbuslockagent.h"
#include "dlineeditex.h"
#include "lockcontent.h"
#include "controlwidget.h"
#include "shutdownwidget.h"
#include "sfa_widget.h"
#include "mfa_widget.h"
#include "mfasequencecontrol.h"
#include "appeventfilter.h"
#include "lockpasswordchangewidget.h"
#include <deepin_pw_check.h>
#include "dbusconstant.h"
#include "deepinauthframework.h"
#include <QDBusVirtualObject>
#include <QDBusVariant>
#include <QClipboard>

#include <QFile>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QScopedValueRollback>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QValidator>
#include <atomic>
#include <memory>
#include <gtest/gtest.h>
#include <security/pam_appl.h>
#include <unistd.h>

class UT_LockPasswordChange : public testing::Test
{
protected:
    QString program(const QByteArray &script)
    {
        const auto path = directory.filePath(QStringLiteral("passwd"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return {};
        }
        file.write("#!/bin/sh\n");
        file.write(script);
        file.close();
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }

    bool wait(const std::function<bool()> &condition)
    {
        return QTest::qWaitFor(condition, 3000);
    }

    QTemporaryDir directory;
};

TEST_F(UT_LockPasswordChange, UnexpiredCompletesWithoutStartingPasswd)
{
    LockPasswordChange change(nullptr, [](const QString &, const QString &) { return PAM_SUCCESS; }, QStringLiteral("/nonexistent/passwd"));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), false, false);
    ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
    EXPECT_FALSE(finished[0][0].toBool());
    EXPECT_FALSE(change.isActive());
}

TEST_F(UT_LockPasswordChange, AccountErrorsNeverAuthorizeUnlock)
{
    for (const auto result : {PAM_SYSTEM_ERR, PAM_USER_UNKNOWN, PAM_PERM_DENIED, PAM_ACCT_EXPIRED}) {
        LockPasswordChange change(nullptr, [result](const QString &, const QString &) { return result; });
        QSignalSpy finished(&change, &LockPasswordChange::completed);
        QSignalSpy canceled(&change, &LockPasswordChange::canceled);
        change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), false, false);
        ASSERT_TRUE(wait([&] { return canceled.count() == 1; }));
        EXPECT_EQ(finished.count(), 0);
        EXPECT_FALSE(change.isActive());
    }
}

TEST_F(UT_LockPasswordChange, PamExpiredResultEntersOriginalInputFlow)
{
    for (const auto result : {PAM_NEW_AUTHTOK_REQD, PAM_AUTHTOK_EXPIRED}) {
        LockPasswordChange change(nullptr, [result](const QString &, const QString &) { return result; });
        QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
        change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), false, false);
        ASSERT_TRUE(wait([&] { return prompts.count() >= 2; }));
        EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the new password."));
        EXPECT_FALSE(prompts.last()[2].toBool());
    }
}

TEST_F(UT_LockPasswordChange, SupportedNewPasswordKeepsUtf8CurrentPasswordAndRequiresConfirmation)
{
    const auto executable = program(u8"IFS= read -r old\nIFS= read -r new\nIFS= read -r confirmation\n"
                                    u8"test \"$old\" = '旧密码🙂' && test \"$new\" = 'New password ~!' && test \"$new\" = \"$confirmation\"\n");
    ASSERT_FALSE(executable.isEmpty());
    LockPasswordChange change(nullptr, {}, executable);
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QString::fromUtf8(u8"旧密码🙂"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("New password ~!"));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Confirm the new password."));
    change.submit(QStringLiteral("different"));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(prompts.last()[1].toString(), QStringLiteral("The two password inputs are inconsistent."));
    change.submit(QStringLiteral("New password ~!"));
    change.submit(QStringLiteral("New password ~!"));
    ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
    EXPECT_TRUE(finished[0][0].toBool());

    AuthSingle pamInput;
    pamInput.resetPasswordChangePrompt();
    AuthPassword factorInput;
    // The existing password-factor validator has no QObject owner.
    const_cast<QValidator *>(factorInput.m_lineEdit->lineEdit()->validator())->setParent(&factorInput);
    for (const auto *validator : {pamInput.m_lineEdit->lineEdit()->validator(), factorInput.m_lineEdit->lineEdit()->validator()}) {
        auto password = QStringLiteral("New password ~!");
        int cursor = password.size();
        EXPECT_EQ(validator->validate(password, cursor), QValidator::Acceptable);
    }
}

TEST_F(UT_LockPasswordChange, UnsupportedNewPasswordIsRejectedBeforeStartingPasswd)
{
    LockPasswordChange change(nullptr, {}, program("exit 0\n"));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    for (const auto &password : {QString::fromUtf8(u8"新密码🙂"), QString::fromUtf8(u8"NewéPassword"), QStringLiteral("new\tpassword")}) {
        change.submit(password);
        change.submit(password);
        EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the new password."));
        EXPECT_FALSE(prompts.last()[1].toString().isEmpty());
        EXPECT_EQ(change.m_process, nullptr);
        EXPECT_EQ(finished.count(), 0);
    }
}

TEST_F(UT_LockPasswordChange, NonPasswordAuthenticationRequestsCurrentPassword)
{
    LockPasswordChange change;
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), {}, true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the old password."));
    change.submit(QStringLiteral("old"));
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the new password."));
    change.submit(QStringLiteral("old"));
    EXPECT_FALSE(prompts.last()[1].toString().isEmpty());
}

TEST_F(UT_LockPasswordChange, UnknownExpiryUsesCurrentPasswordForPamCheck)
{
    LockPasswordChange change(nullptr, [](const QString &account, const QString &password) {
        return account == QStringLiteral("test-domain-user") && password == QStringLiteral("test-old-password")
            ? PAM_NEW_AUTHTOK_REQD : PAM_AUTH_ERR;
    });
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), {}, false, false);
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the old password."));
    change.submit(QStringLiteral("test-old-password"));
    ASSERT_TRUE(wait([&] { return prompts.last()[0].toString() == QStringLiteral("Please enter the new password."); }));
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordChange, PasswordlessAccountChecksEmptyCredentialDespiteCachedExpiry)
{
    LockPasswordChange change(nullptr, [](const QString &account, const QString &password) {
        return account == QStringLiteral("passwordless-user") && password.isEmpty() ? PAM_SUCCESS : PAM_AUTH_ERR;
    }, QStringLiteral("/nonexistent/passwd"));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy forms(&change, &LockPasswordChange::formChanged);
    change.begin(QStringLiteral("passwordless-user"), QStringLiteral("stale-cache"), true, false, true);
    ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
    EXPECT_FALSE(finished[0][0].toBool());
    EXPECT_FALSE(change.isActive());
    for (const auto &form : forms)
        EXPECT_FALSE(form[0].toBool());

    // Reusing the controller for a password-bearing account must not retain
    // the previous account's known-empty credential.
    change.begin(QStringLiteral("password-user"), {}, false, false);
    EXPECT_TRUE(forms.last()[0].toBool());
    change.submit({});
    EXPECT_TRUE(change.acceptsInput());
    EXPECT_EQ(finished.count(), 1);
}

TEST_F(UT_LockPasswordChange, PasswordlessAccountErrorsNeverAuthorizeUnlock)
{
    for (const int result : {PAM_AUTH_ERR, PAM_USER_UNKNOWN, PAM_PERM_DENIED, PAM_ACCT_EXPIRED, PAM_SYSTEM_ERR}) {
        LockPasswordChange change(nullptr, [result](const QString &, const QString &password) {
            return password.isEmpty() ? result : PAM_SUCCESS;
        });
        QSignalSpy finished(&change, &LockPasswordChange::completed);
        QSignalSpy canceled(&change, &LockPasswordChange::canceled);
        change.begin(QStringLiteral("passwordless-user"), {}, false, false, true);
        ASSERT_TRUE(wait([&] { return canceled.count() == 1; }));
        EXPECT_EQ(finished.count(), 0);
        EXPECT_FALSE(change.isActive());
    }
}

TEST_F(UT_LockPasswordChange, PasswordlessExpirySubmitsOnlyNewPasswordAndConfirmation)
{
    const auto executable = program("IFS= read -r new\nIFS= read -r confirmation\n"
                                    "test \"$new\" = 'newSecret!' && test \"$confirmation\" = \"$new\"\n");
    LockPasswordChange change(nullptr, [](const QString &, const QString &password) {
        return password.isEmpty() ? PAM_NEW_AUTHTOK_REQD : PAM_AUTH_ERR;
    }, executable);
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy forms(&change, &LockPasswordChange::formChanged);
    change.begin(QStringLiteral("passwordless-user"), {}, false, false, true);
    ASSERT_TRUE(wait([&] { return change.acceptsInput(); }));
    EXPECT_FALSE(forms.last()[0].toBool());
    change.submitPasswords({}, {}, {});
    EXPECT_EQ(change.m_process, nullptr);
    change.submitPasswords({}, QStringLiteral("newSecret!"), QStringLiteral("newSecret!"));
    ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
    EXPECT_TRUE(finished[0][0].toBool());
}

TEST_F(UT_LockPasswordChange, PasswordlessSuspendRequiresFreshActionAndDiscardsPendingCheck)
{
    LockPasswordChange change(nullptr, [](const QString &, const QString &) {
        QThread::msleep(30);
        return PAM_SUCCESS;
    });
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy canceled(&change, &LockPasswordChange::canceled);
    change.begin(QStringLiteral("passwordless-user"), {}, false, false, true);
    change.restartAtCurrentPassword();
    EXPECT_FALSE(change.isActive());
    EXPECT_EQ(canceled.count(), 1);
    ASSERT_TRUE(wait([&] { return !change.m_checkThread || !change.m_checkThread->isRunning(); }));
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordChange, ProcessFailureIsRetryableAndDoesNotExposePasswords)
{
    const auto executable = program("IFS= read -r old\nIFS= read -r new\nIFS= read -r confirmation\n"
                                    "printf '%s %s policy rejected\\n' \"$old\" \"$new\"\nexit 1\n");
    LockPasswordChange change(nullptr, {}, executable);
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("test-old-password"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("test-new-password"));
    change.submit(QStringLiteral("test-new-password"));
    ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the old password."));
    EXPECT_FALSE(prompts.last()[1].toString().contains(QStringLiteral("test-old-password")));
    EXPECT_FALSE(prompts.last()[1].toString().contains(QStringLiteral("test-new-password")));
}

TEST_F(UT_LockPasswordChange, ProcessFailureRedactsOverlappingPasswords)
{
    const auto executable = program("IFS= read -r old\nIFS= read -r new\nIFS= read -r confirmation\n"
                                    "printf '%s|%s|abcd|policy rejected\\n' \"$old\" \"$new\"\nexit 1\n");
    const QList<QPair<QString, QString>> passwords {
        {QStringLiteral("old"), QStringLiteral("oldSecret!")},
        {QStringLiteral("oldSecret!"), QStringLiteral("old")},
        {QStringLiteral("abc"), QStringLiteral("bcd")},
        {QStringLiteral("redacted"), QStringLiteral("[redacted]Secret!")}
    };
    for (const auto &pair : passwords) {
        LockPasswordChange change(nullptr, {}, executable);
        QSignalSpy finished(&change, &LockPasswordChange::completed);
        QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
        change.begin(QStringLiteral("test-domain-user"), pair.first, true, false);
        ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::NewPassword; }));
        change.submit(pair.second);
        change.submit(pair.second);
        ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
        const auto detail = pair.first == QStringLiteral("abc") ? QStringLiteral("[redacted]") : QStringLiteral("abcd");
        EXPECT_EQ(prompts.last()[1].toString(), QStringLiteral("[redacted]|[redacted]|") + detail + QStringLiteral("|policy rejected"));
        EXPECT_EQ(finished.count(), 0);
    }
}

TEST_F(UT_LockPasswordChange, CancelDiscardsLateAccountReply)
{
    LockPasswordChange change(nullptr, [](const QString &, const QString &) { QThread::msleep(50); return PAM_SUCCESS; });
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    change.begin(QStringLiteral("old-user"), QStringLiteral("old"), false, false);
    change.cancel();
    QTest::qWait(100);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(change.isActive());
}

TEST_F(UT_LockPasswordChange, MissingPasswdNeverReportsSuccess)
{
    LockPasswordChange change(nullptr, {}, directory.filePath(QStringLiteral("missing-passwd")));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("new"));
    change.submit(QStringLiteral("new"));
    ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(change.isActive());
}

TEST_F(UT_LockPasswordChange, CrashedPasswdNeverReportsSuccess)
{
    LockPasswordChange change(nullptr, {}, program("kill -KILL $$\n"));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("new"));
    change.submit(QStringLiteral("new"));
    ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordChange, RunningPasswdIsStoppedOnTimeoutOrCancel)
{
    LockPasswordChange change(nullptr, {}, program("exec sleep 30\n"));
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("new"));
    change.submit(QStringLiteral("new"));
    ASSERT_TRUE(wait([&] { return change.m_process && change.m_process->state() == QProcess::Running; }));
    change.m_timer.start(1);
    change.resetIdleTimer();
    EXPECT_EQ(change.m_timer.interval(), 1);
    ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(change.m_process, nullptr);
    change.submit(QStringLiteral("new"));
    change.submit(QStringLiteral("new"));
    change.cancel();
    QTest::qWait(50);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(change.m_process, nullptr);
    EXPECT_FALSE(change.isActive());
}

TEST_F(UT_LockPasswordChange, CancelAndTimeoutNeverReportSuccess)
{
    for (bool timeout : {false, true}) {
        SCOPED_TRACE(timeout);
        LockPasswordChange change;
        QSignalSpy finished(&change, &LockPasswordChange::completed);
        QSignalSpy canceled(&change, &LockPasswordChange::canceled);
        QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
        change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
        ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
        change.submit(QStringLiteral("new"));
        if (timeout)
            change.m_timer.start(1);
        else
            change.cancel();
        ASSERT_TRUE(wait([&] { return canceled.count() == 1; }));
        EXPECT_EQ(finished.count(), 0);
        EXPECT_FALSE(change.isActive());
        EXPECT_FALSE(change.m_timer.isActive());
        EXPECT_TRUE(change.m_account.isEmpty());
        EXPECT_TRUE(change.m_oldPassword.isEmpty());
        EXPECT_TRUE(change.m_newPassword.isEmpty());
        change.cancel();
        EXPECT_EQ(canceled.count(), 1);
    }
}

TEST_F(UT_LockPasswordChange, LineBreakCannotBecomeAnExtraPasswdResponse)
{
    LockPasswordChange change;
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("old"), true, false);
    ASSERT_TRUE(wait([&] { return prompts.count() == 2; }));
    change.submit(QStringLiteral("new\nextra"));
    EXPECT_EQ(prompts.last()[0].toString(), QStringLiteral("Please enter the new password."));
    EXPECT_FALSE(prompts.last()[1].toString().isEmpty());
}

TEST_F(UT_LockPasswordChange, CompleteFormValidatesBeforeConsumingCurrentPassword)
{
    const auto executable = program("IFS= read -r old\nIFS= read -r new\nIFS= read -r confirmation\n"
                                    "test \"$old\" = 'old' && test \"$new\" = 'new' && test \"$new\" = \"$confirmation\"\n");
    LockPasswordChange change(nullptr, {}, executable);
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
    change.begin(QStringLiteral("test-domain-user"), {}, true, false);
    ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::CurrentPassword; }));
    change.submitPasswords(QStringLiteral("old"), QStringLiteral("new"), QStringLiteral("different"));
    EXPECT_EQ(prompts.last()[1].toString(), QStringLiteral("The two password inputs are inconsistent."));
    EXPECT_EQ(change.m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_TRUE(change.m_oldPassword.isEmpty());
    EXPECT_EQ(change.m_process, nullptr);
    change.submitPasswords(QStringLiteral("old"), QString::fromUtf8(u8"新密码🙂"), QString::fromUtf8(u8"新密码🙂"));
    EXPECT_EQ(change.m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_EQ(change.m_process, nullptr);
    change.submitPasswords(QStringLiteral("old"), QStringLiteral("new"), QStringLiteral("new"), QStringLiteral("password is new"));
    EXPECT_EQ(change.m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_EQ(change.m_process, nullptr);
    EXPECT_TRUE(change.m_oldPassword.isEmpty());
    change.submitPasswords(QStringLiteral("old"), QStringLiteral("new"), QStringLiteral("new"));
    ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
    EXPECT_TRUE(finished[0][0].toBool());
}

TEST_F(UT_LockPasswordChange, RestartRequiresFreshCurrentPasswordVerification)
{
    QString checkedPassword;
    LockPasswordChange change(nullptr, [&checkedPassword](const QString &, const QString &password) {
        checkedPassword = password;
        return PAM_NEW_AUTHTOK_REQD;
    });
    QSignalSpy cleared(&change, &LockPasswordChange::inputsCleared);
    QSignalSpy finished(&change, &LockPasswordChange::completed);
    change.begin(QStringLiteral("test-domain-user"), QStringLiteral("cached-old"), true, false);
    ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::NewPassword; }));
    change.submit(QStringLiteral("unsubmitted-new"));
    change.restartAtCurrentPassword();
    EXPECT_EQ(cleared.count(), 1);
    EXPECT_TRUE(change.m_oldPassword.isEmpty());
    EXPECT_TRUE(change.m_newPassword.isEmpty());
    EXPECT_EQ(change.m_stage, LockPasswordChange::CurrentPassword);
    change.submitPasswords(QStringLiteral("fresh-old"), {}, {});
    ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::NewPassword; }));
    EXPECT_EQ(checkedPassword, QStringLiteral("fresh-old"));
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordChange, SleepRestartDiscardsStartingAndRunningPasswdResults)
{
    const auto executable = program("exec sleep 30\n");
    ASSERT_FALSE(executable.isEmpty());
    for (bool waitForStarted : {false, true}) {
        LockPasswordChange change(nullptr, [](const QString &, const QString &password) {
            // passwd could have committed the new password before suspension.
            return password == QStringLiteral("new-current") ? PAM_SUCCESS : PAM_AUTH_ERR;
        }, executable);
        QSignalSpy finished(&change, &LockPasswordChange::completed);
        QSignalSpy cleared(&change, &LockPasswordChange::inputsCleared);
        change.begin(QStringLiteral("test-domain-user"), QStringLiteral("cached-old"), true, false);
        ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::NewPassword; }));
        change.submitPasswords({}, QStringLiteral("new-current"), QStringLiteral("new-current"));
        ASSERT_EQ(change.m_stage, LockPasswordChange::ChangingPassword);
        QPointer<QProcess> process = change.m_process;
        ASSERT_TRUE(process);
        if (waitForStarted) {
            ASSERT_TRUE(wait([&] { return process && process->state() == QProcess::Running; }));
        }
        change.restartAtCurrentPassword();
        EXPECT_EQ(change.m_process, nullptr);
        EXPECT_EQ(change.m_stage, LockPasswordChange::CurrentPassword);
        EXPECT_EQ(cleared.count(), 1);
        EXPECT_TRUE(change.m_oldPassword.isEmpty());
        EXPECT_TRUE(change.m_newPassword.isEmpty());
        EXPECT_TRUE(change.m_output.isEmpty());
        // Even an already queued successful result from the interrupted process
        // cannot authorize unlock after callbacks have been disconnected.
        emit process->finished(0, QProcess::NormalExit);
        ASSERT_TRUE(wait([&] { return !process || process->state() == QProcess::NotRunning; }));
        EXPECT_EQ(finished.count(), 0);
        EXPECT_TRUE(change.isActive());
        change.submitPasswords(QStringLiteral("new-current"), {}, {});
        ASSERT_TRUE(wait([&] { return finished.count() == 1; }));
        EXPECT_FALSE(finished[0][0].toBool());
    }
}

class ValidLockTestUser : public User
{
public:
    bool isUserValid() const override { return true; }
    int type() const override { return ADDomain; }
    ValidLockTestUser()
    {
        m_uid = getuid();
        m_name = QStringLiteral("test-domain-user");
    }
};

class AccountInfoLockTestUser : public ValidLockTestUser
{
public:
    AccountInfoLockTestUser() { m_name = QStringLiteral("test-account-info-user"); }
    int type() const override { return Native; }
    QString path() const override { return QString(DSS_DBUS::accountsUserPath).arg(uid()); }
};

class AccountExpiryTestService : public QDBusVirtualObject
{
public:
    const QString path = QString(DSS_DBUS::accountsUserPath).arg(getuid());
    int state = User::ExpiredNormal;
    int requests = 0;
    bool deferReply = false;
    bool wrongType = false;
    bool incomplete = false;
    QString error;
    QList<QDBusMessage> pending;

    ~AccountExpiryTestService() override
    {
        QDBusConnection::systemBus().unregisterObject(path);
        QDBusConnection::systemBus().unregisterService(DSS_DBUS::accountsService);
    }
    bool registerOnBus()
    {
        auto bus = QDBusConnection::systemBus();
        return bus.registerService(DSS_DBUS::accountsService) && bus.registerVirtualObject(path, this);
    }
    QString introspect(const QString &) const override
    {
        return QStringLiteral("<interface name=\"%1\"><method name=\"PasswordExpiredInfo\">"
                              "<arg type=\"i\" direction=\"out\"/><arg type=\"x\" direction=\"out\"/>"
                              "</method></interface>").arg(DSS_DBUS::accountsUserInterface);
    }
    QDBusMessage reply(const QDBusMessage &request) const
    {
        if (!error.isEmpty())
            return request.createErrorReply(error, QStringLiteral("test expiry lookup failed"));
        QVariantList values {wrongType ? QVariant(QStringLiteral("normal")) : QVariant(state)};
        if (!incomplete)
            values << QVariant::fromValue<qlonglong>(1);
        return request.createReply(values);
    }
    bool handleMessage(const QDBusMessage &request, const QDBusConnection &bus) override
    {
        if (request.interface() != DSS_DBUS::accountsUserInterface
            || request.member() != QStringLiteral("PasswordExpiredInfo"))
            return false;
        ++requests;
        if (deferReply) {
            request.setDelayedReply(true);
            pending << request;
        } else {
            bus.send(reply(request));
        }
        return true;
    }
    void answerPending()
    {
        for (const auto &request : pending)
            QDBusConnection::systemBus().send(reply(request));
        pending.clear();
    }
};

class UT_LockPasswordGate : public testing::Test
{
protected:
    void SetUp() override
    {
        // No test in this suite may talk to real accounts, authenticate or change a password.
        if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Run with tests/dde-lock/run-lock-password-tests.sh";
        }
        model = new SessionBaseModel;
        model->setAppType(AuthCommon::Lock);
        worker = new LockWorker(model);
        model->updateCurrentUser(std::make_shared<ValidLockTestUser>());
        worker->m_account = model->currentUser()->name();
        worker->m_forcePasswordChange = true;
        worker->m_passwordChange->m_checker = [](const QString &, const QString &) { return PAM_NEW_AUTHTOK_REQD; };
    }
    void TearDown() override
    {
        // Let existing asynchronous framework startup watchers finish before
        // their receiver is destroyed, and drain deferred process/thread cleanup.
        QTest::qWait(20);
        delete worker;
        delete model;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    std::unique_ptr<LockContent> managedContent()
    {
        model->setAuthType(AT_PAM);
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        auto content = std::make_unique<LockContent>();
        // Exercise actual page selection/connections without opening the lock
        // socket or starting a virtual keyboard on the host.
        content->m_model = model;
        content->initUI();
        content->initConnections();
        QObject::connect(content.get(), &LockContent::requestChangePassword, worker, &LockWorker::submitPasswordChange);
        QObject::connect(content.get(), &LockContent::requestCancelPasswordChange, worker, &LockWorker::cancelPasswordChange);
        QObject::connect(content.get(), &LockContent::passwordChangeUserIsActive, worker, &LockWorker::restartResetSessionTimer);
        QObject::disconnect(model, &SessionBaseModel::hasVirtualKBChanged, content.get(), nullptr);
        content->m_shutdownFrame->setParent(content.get());
        content->m_shutdownFrame->hide();
        content->setFullManagedLoginWidget(new QWidget(content.get()));
        content->pushPasswordFrame();
        return content;
    }
    SessionBaseModel *model = nullptr;
    LockWorker *worker = nullptr;
};

class AuthenticationLookupTestService : public QDBusVirtualObject
{
public:
    int lookups = 0;
    ~AuthenticationLookupTestService() override
    {
        QDBusConnection::systemBus().unregisterObject(DSS_DBUS::accountsPath);
        QDBusConnection::systemBus().unregisterService(DSS_DBUS::accountsService);
    }
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.interface() != DSS_DBUS::accountsService || message.member() != QStringLiteral("FindUserByName"))
            return false;
        ++lookups;
        // Observe createAuthentication's first backend request, then reject it
        // before a real PAM thread or authentication controller can start.
        bus.send(message.createReply(QVariantList {QString()}));
        return true;
    }
};

class SleepLockTestService : public QDBusVirtualObject
{
public:
    ~SleepLockTestService() override
    {
        QDBusConnection::sessionBus().unregisterObject(DSS_DBUS::sessionPowerPath);
        QDBusConnection::sessionBus().unregisterService(DSS_DBUS::sessionPowerService);
    }
    QString introspect(const QString &) const override
    {
        return QStringLiteral("<interface name=\"%1\"><property name=\"SleepLock\" type=\"b\" access=\"read\"/></interface>")
            .arg(DSS_DBUS::sessionPowerService);
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.interface() != QStringLiteral("org.freedesktop.DBus.Properties")
            || message.member() != QStringLiteral("Get")
            || message.arguments().value(1).toString() != QStringLiteral("SleepLock"))
            return false;
        bus.send(message.createReply(QVariantList {QVariant::fromValue(QDBusVariant(false))}));
        return true;
    }
};

class LoginSessionTestService : public QDBusVirtualObject
{
public:
    const QString service = QStringLiteral("org.freedesktop.login1");
    const QString managerPath = QStringLiteral("/org/freedesktop/login1");
    const QString sessionPath = QStringLiteral("/org/freedesktop/login1/session/lock_test");
    bool active = true;

    QString introspect(const QString &path) const override
    {
        if (path == managerPath) {
            return QStringLiteral("<interface name=\"org.freedesktop.login1.Manager\">"
                                  "<method name=\"GetSessionByPID\"><arg type=\"u\" direction=\"in\"/>"
                                  "<arg type=\"o\" direction=\"out\"/></method></interface>");
        }
        return QStringLiteral("<interface name=\"org.freedesktop.login1.Session\">"
                              "<property name=\"Active\" type=\"b\" access=\"read\"/></interface>");
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.path() == managerPath && message.member() == QStringLiteral("GetSessionByPID")) {
            bus.send(message.createReply(QVariantList {QVariant::fromValue(QDBusObjectPath(sessionPath))}));
            return true;
        }
        if (message.path() != sessionPath || message.interface() != QStringLiteral("org.freedesktop.DBus.Properties"))
            return false;
        if (message.member() == QStringLiteral("GetAll")) {
            bus.send(message.createReply(QVariantList {QVariantMap {{QStringLiteral("Active"), active}}}));
            return true;
        }
        if (message.member() == QStringLiteral("Get") && message.arguments().value(1).toString() == QStringLiteral("Active")) {
            bus.send(message.createReply(QVariantList {QVariant::fromValue(QDBusVariant(active))}));
            return true;
        }
        return false;
    }
    void setActive(bool value)
    {
        active = value;
        auto message = QDBusMessage::createSignal(sessionPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                QStringLiteral("PropertiesChanged"));
        message << QStringLiteral("org.freedesktop.login1.Session")
                << QVariantMap {{QStringLiteral("Active"), active}} << QStringList();
        QDBusConnection::systemBus().send(message);
    }
};

class UT_LockPasswordSession : public UT_LockPasswordGate
{
protected:
    void SetUp() override
    {
        if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
            GTEST_SKIP() << "Requires the isolated test bus";
        auto bus = QDBusConnection::systemBus();
        ASSERT_TRUE(bus.registerService(session.service));
        ASSERT_TRUE(bus.registerVirtualObject(session.managerPath, &session, QDBusConnection::SubPath));
        UT_LockPasswordGate::SetUp();
        ASSERT_NE(worker->m_login1SessionSelf, nullptr);
        ASSERT_TRUE(worker->m_login1SessionSelf->isValid());
        QTest::qWait(20);
    }
    void TearDown() override
    {
        UT_LockPasswordGate::TearDown();
        auto bus = QDBusConnection::systemBus();
        bus.unregisterObject(session.managerPath, QDBusConnection::UnregisterTree);
        bus.unregisterService(session.service);
    }
    bool setActive(bool active)
    {
        QSignalSpy changed(worker->m_login1SessionSelf, &Login1SessionSelf::ActiveChanged);
        session.setActive(active);
        // Authentication startup can report the initial Active value before
        // PropertiesChanged arrives. Wait for the requested session state.
        return QTest::qWaitFor([&] {
            return !changed.isEmpty() && changed.constLast().at(0).toBool() == active;
        }, 1000);
    }
    LoginSessionTestService session;
};

TEST_F(UT_LockPasswordSession, DeactivationClearsFormAndRestartsOnlyAfterActivation)
{
    AuthenticationLookupTestService lookup;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &lookup));
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("cached-old");
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return model->lockPasswordChangePageVisible() && worker->m_passwordChange->acceptsInput();
    }, 3000));
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    page->m_localSecurityCheck = false;
    page->m_password->setText(QStringLiteral("draft-new"));
    page->m_confirmation->setText(QStringLiteral("draft-new"));
    page->m_hint->setText(QStringLiteral("draft-hint"));
    worker->m_passwordChangeHint = QStringLiteral("cached-hint");
    worker->m_password = QStringLiteral("cached-auth");
    worker->m_resetSessionTimer->start();
    lookup.lookups = 0;
    QSignalSpy finished(model, &SessionBaseModel::authFinished);

    ASSERT_TRUE(setActive(false));
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_TRUE(worker->m_passwordChangeHint.isEmpty());
    EXPECT_TRUE(worker->m_password.isEmpty());
    EXPECT_TRUE(page->m_current->text().isEmpty());
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_TRUE(page->m_confirmation->text().isEmpty());
    EXPECT_TRUE(page->m_hint->text().isEmpty());
    EXPECT_FALSE(worker->m_resetSessionTimer->isActive());
    EXPECT_TRUE(model->lockPasswordChangeProtected());
    model->setVisible(false);
    EXPECT_TRUE(model->visible());
    // Queued cancellation, explicit refreshes and late token callbacks must
    // not recreate an authentication session or cache credentials off the VT.
    worker->createAuthentication(model->currentUser()->name());
    worker->sendTokenToAuth(model->currentUser()->name(), AT_Password, QStringLiteral("late-token"));
    worker->authFinishedAction();
    QTest::qWait(40);
    EXPECT_EQ(lookup.lookups, 0);
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_EQ(finished.count(), 0);

    ASSERT_TRUE(setActive(true));
    ASSERT_TRUE(QTest::qWaitFor([&] { return lookup.lookups == 1; }, 1000));
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return worker->m_passwordChange->acceptsInput(); }, 3000));
    EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordSession, DeactivationRejectsPamSuccessBeforeAndAfterActivation)
{
    for (bool replyWhileInactive : {true, false}) {
        SCOPED_TRACE(replyWhileInactive);
        auto release = std::make_shared<QSemaphore>();
        auto entered = std::make_shared<QSemaphore>();
        worker->m_passwordChangePassed = false;
        worker->m_passwordChangeToken = QStringLiteral("cached-old");
        worker->m_passwordChange->m_checker = [release, entered](const QString &, const QString &password) {
            if (password == QStringLiteral("cached-old")) {
                entered->release();
                release->acquire();
            }
            return PAM_SUCCESS;
        };
        const auto unblockOnExit = qScopeGuard([release] { release->release(); });
        QSignalSpy finished(model, &SessionBaseModel::authFinished);
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return entered->tryAcquire(); }, 3000));
        QPointer<QThread> pending = worker->m_passwordChange->m_checkThread;
        ASSERT_TRUE(setActive(false));
        EXPECT_FALSE(worker->m_passwordChange->isActive());
        EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
        if (!replyWhileInactive) {
            ASSERT_TRUE(setActive(true));
        }
        release->release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !pending || !pending->isRunning(); }, 3000));
        QTest::qWait(20);
        EXPECT_EQ(finished.count(), 0);
        EXPECT_FALSE(worker->m_passwordChangePassed);
        EXPECT_TRUE(model->lockPasswordChangeProtected());
        if (replyWhileInactive) {
            ASSERT_TRUE(setActive(true));
        }
        worker->authFinishedAction();
        ASSERT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
        worker->submitPasswordChange(QStringLiteral("fresh-current"), {}, {}, {});
        ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
        EXPECT_TRUE(finished[0][0].toBool());
        EXPECT_FALSE(model->lockPasswordChangeProtected());
    }
}

TEST_F(UT_LockPasswordSession, RapidReactivationDoesNotReplayQueuedCancellation)
{
    AuthenticationLookupTestService lookup;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &lookup));
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("cached-old");
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return model->lockPasswordChangePageVisible() && worker->m_passwordChange->acceptsInput();
    }, 3000));
    lookup.lookups = 0;
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    // Deliver both notifications before the queued cancellation recovery runs.
    emit worker->m_login1SessionSelf->ActiveChanged(false);
    emit worker->m_login1SessionSelf->ActiveChanged(true);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    QTest::qWait(20);
    EXPECT_EQ(lookup.lookups, 0);
    ASSERT_TRUE(QTest::qWaitFor([&] { return lookup.lookups == 1; }, 1000));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(model->lockPasswordChangeProtected());
    EXPECT_FALSE(worker->m_passwordChangePassed);
}

TEST_F(UT_LockPasswordSession, DeactivationRejectsLateAccountInfoSuccess)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    service.deferReply = true;
    ASSERT_TRUE(model->updateCurrentUser(std::make_shared<AccountInfoLockTestUser>()));
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    for (bool replyWhileInactive : {true, false}) {
        SCOPED_TRACE(replyWhileInactive);
        // Start a fresh attempt, then deactivate while the Accounts reply is pending.
        model->setLockPasswordChangeRequiresAuthentication(false);
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !service.pending.isEmpty(); }, 1000));
        QPointer<QThread> thread = worker->m_passwordChange->m_checkThread;
        ASSERT_TRUE(setActive(false));
        if (!replyWhileInactive) {
            ASSERT_TRUE(setActive(true));
        }
        service.answerPending();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !thread || !thread->isRunning(); }, 3000));
        QTest::qWait(20);
        EXPECT_EQ(finished.count(), 0);
        EXPECT_FALSE(worker->m_passwordChangePassed);
        EXPECT_FALSE(worker->m_passwordChange->isActive());
        EXPECT_TRUE(model->lockPasswordChangeProtected());
        if (replyWhileInactive) {
            ASSERT_TRUE(setActive(true));
        }
    }
}

TEST_F(UT_LockPasswordGate, SingleFactorBiometricSuccessDiscardsUnusedPasswordToken)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    worker->m_account = user->name();
    const auto pamChecks = std::make_shared<std::atomic<int>>(0);
    worker->m_passwordChange->m_checker = [pamChecks](const QString &, const QString &) {
        ++*pamChecks;
        return PAM_AUTH_ERR;
    };
    for (const auto factor : {AT_Face, AT_Iris, AT_Fingerprint}) {
        SCOPED_TRACE(factor);
        worker->m_passwordChangePassed = false;
        model->setVisible(true);
        model->updateMFAFlag(false);
        // No PAM worker consumes this submission; it remains an unverified candidate.
        model->updateFrameworkState(Unavailable);
        ASSERT_FALSE(worker->m_authFramework->IsUsingPamAuth());
        worker->sendTokenToAuth(user->name(), AT_Password, QStringLiteral("unverified-candidate"));
        ASSERT_EQ(worker->m_passwordChangeToken, QStringLiteral("unverified-candidate"));
        QSignalSpy finished(model, &SessionBaseModel::authFinished);
        const int previousRequests = service.requests;
        worker->onAuthStateChanged(factor, AS_Success, {});
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !worker->m_passwordChange->isActive(); }, 3000));
        ASSERT_EQ(finished.count(), 1);
        EXPECT_TRUE(finished[0][0].toBool());
        EXPECT_EQ(service.requests, previousRequests + 1);
        EXPECT_EQ(pamChecks->load(), 0);
        EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
        EXPECT_FALSE(model->lockPasswordChangePageVisible());
    }
}

TEST_F(UT_LockPasswordGate, MfaBiometricSuccessPreservesVerifiedPasswordToken)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    worker->m_account = user->name();
    model->updateMFAFlag(true);
    model->updateFrameworkState(Unavailable);
    ASSERT_FALSE(worker->m_authFramework->IsUsingPamAuth());
    worker->sendTokenToAuth(user->name(), AT_Password, QStringLiteral("verified-current"));
    const auto pamChecks = std::make_shared<std::atomic<int>>(0);
    worker->m_passwordChange->m_checker = [pamChecks](const QString &, const QString &password) {
        ++*pamChecks;
        return password == QStringLiteral("verified-current") ? PAM_SUCCESS : PAM_AUTH_ERR;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->onAuthStateChanged(AT_Password, AS_Success, {});
    worker->onAuthStateChanged(AT_Face, AS_Success, {});
    EXPECT_EQ(worker->m_passwordChangeToken, QStringLiteral("verified-current"));
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(pamChecks->load(), 0);
    EXPECT_EQ(finished.count(), 0);
    worker->onAuthStateChanged(AT_All, AS_Success, {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(pamChecks->load(), 1);
    EXPECT_EQ(service.requests, 0);
}

TEST_F(UT_LockPasswordGate, BiometricSuccessAfterCancellationStillRequiresFreshPassword)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    worker->m_account = user->name();
    model->updateMFAFlag(false);
    model->updateFrameworkState(Unavailable);
    ASSERT_FALSE(worker->m_authFramework->IsUsingPamAuth());
    worker->sendTokenToAuth(user->name(), AT_Password, QStringLiteral("unverified-candidate"));
    model->setLockPasswordChangeRequiresAuthentication(true);
    const auto pamChecks = std::make_shared<std::atomic<int>>(0);
    worker->m_passwordChange->m_checker = [pamChecks](const QString &, const QString &) {
        ++*pamChecks;
        return PAM_AUTH_ERR;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->onAuthStateChanged(AT_Face, AS_Success, {});
    worker->authFinishedAction();
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_TRUE(worker->m_passwordChange->acceptsInput());
    EXPECT_TRUE(model->lockPasswordChangeProtected());
    EXPECT_EQ(pamChecks->load(), 0);
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(finished.count(), 0);
    worker->cancelPasswordChange();
}

TEST_F(UT_LockPasswordGate, TokenDeliveryFailureDiscardsPasswordBeforeBiometricUnlock)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    worker->m_account = user->name();
    model->updateMFAFlag(false);
    model->updateFrameworkState(Available);
    const auto pamChecks = std::make_shared<std::atomic<int>>(0);
    worker->m_passwordChange->m_checker = [pamChecks](const QString &, const QString &) {
        ++*pamChecks;
        return PAM_AUTH_ERR;
    };
    QSignalSpy failed(worker->m_authFramework, &DeepinAuthFramework::TokenSendFailed);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->sendTokenToAuth(user->name(), AT_Password, QStringLiteral("unverified-candidate"));
    ASSERT_EQ(failed.count(), 1);
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    worker->onAuthStateChanged(AT_Face, AS_Success, {});
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(service.requests, 1);
    EXPECT_EQ(pamChecks->load(), 0);
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
}

TEST_F(UT_LockPasswordGate, BiometricWithConfirmedUnexpiredPasswordSkipsPasswordInput)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    auto content = managedContent();
    int pamChecks = 0;
    worker->m_passwordChange->m_checker = [&pamChecks](const QString &, const QString &) {
        ++pamChecks;
        return PAM_AUTH_ERR;
    };
    for (auto state : {User::ExpiredNormal, User::ExpiredSoon}) {
        for (auto factor : {AT_Face, AT_Iris, AT_Fingerprint}) {
            SCOPED_TRACE(state);
            SCOPED_TRACE(factor);
            service.state = state;
            worker->m_passwordChangePassed = false;
            model->setVisible(true);
            model->updateMFAFlag(false);
            worker->onAuthStateChanged(factor, AS_Success, {});
            QSignalSpy finished(model, &SessionBaseModel::authFinished);
            QSignalSpy forms(model, &SessionBaseModel::lockPasswordChangeForm);
            const int previousRequests = service.requests;
            // Face/iris UI waits for Enter before reporting authentication completion.
            worker->authFinishedAction();
            ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
            EXPECT_TRUE(finished[0][0].toBool());
            EXPECT_EQ(service.requests, previousRequests + 1);
            EXPECT_EQ(pamChecks, 0);
            EXPECT_FALSE(worker->m_passwordChange->isActive());
            EXPECT_FALSE(model->lockPasswordChangePageVisible());
            EXPECT_FALSE(model->visible());
            for (const auto &form : forms)
                EXPECT_TRUE(form[2].toBool()); // A slow query may show busy UI, never password input.
        }
    }
}

TEST_F(UT_LockPasswordGate, MfaAccountInfoCheckWaitsForOverallAuthenticationSuccess)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    ASSERT_TRUE(model->updateCurrentUser(std::make_shared<AccountInfoLockTestUser>()));
    model->updateMFAFlag(true);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->onAuthStateChanged(AT_Face, AS_Success, {});
    QTest::qWait(20);
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(finished.count(), 0);
    worker->onAuthStateChanged(AT_All, AS_Success, {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_EQ(service.requests, 1);
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
}

TEST_F(UT_LockPasswordGate, AccountInfoAlreadyExpiredOverridesNormalCache)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    service.state = User::ExpiredAlready;
    ASSERT_TRUE(model->updateCurrentUser(std::make_shared<AccountInfoLockTestUser>()));
    ASSERT_EQ(model->currentUser()->expiredState(), User::ExpiredNormal);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy forms(model, &SessionBaseModel::lockPasswordChangeForm);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !forms.isEmpty() && !forms.last()[2].toBool(); }, 3000));
    EXPECT_EQ(service.requests, 1);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(forms.last()[0].toBool());
    EXPECT_FALSE(forms.last()[1].toBool());
    EXPECT_TRUE(model->lockPasswordChangeProtected());
}

TEST_F(UT_LockPasswordGate, FailedOrMalformedAccountInfoRequiresPasswordAndPam)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    ASSERT_TRUE(model->updateCurrentUser(std::make_shared<AccountInfoLockTestUser>()));
    for (int failure = 0; failure < 4; ++failure) {
        SCOPED_TRACE(failure);
        service.error = failure == 0 ? QStringLiteral("org.freedesktop.DBus.Error.AccessDenied") : QString();
        service.state = failure == 1 ? 99 : User::ExpiredNormal;
        service.wrongType = failure == 2;
        service.incomplete = failure == 3;
        worker->m_passwordChangePassed = false;
        int pamChecks = 0;
        worker->m_passwordChange->m_checker = [&pamChecks](const QString &, const QString &password) {
            ++pamChecks;
            return password == QStringLiteral("fresh-current") ? PAM_SUCCESS : PAM_AUTH_ERR;
        };
        QSignalSpy finished(model, &SessionBaseModel::authFinished);
        const int previousRequests = service.requests;
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return worker->m_passwordChange->acceptsInput(); }, 3000));
        EXPECT_EQ(service.requests, previousRequests + 1);
        EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
        EXPECT_TRUE(worker->m_passwordChange->m_checkAfterCurrentPassword);
        EXPECT_EQ(finished.count(), 0);
        EXPECT_EQ(pamChecks, 0);
        worker->submitPasswordChange(QStringLiteral("fresh-current"), {}, {}, {});
        ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
        EXPECT_EQ(pamChecks, 1);
        EXPECT_TRUE(finished[0][0].toBool());
    }
}

TEST_F(UT_LockPasswordGate, AccountInfoCannotReplacePasswordAuthenticationOrResumeCheck)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    auto user = std::make_shared<AccountInfoLockTestUser>();
    ASSERT_TRUE(model->updateCurrentUser(user));
    int pamChecks = 0;
    worker->m_passwordChange->m_checker = [&pamChecks](const QString &, const QString &) {
        ++pamChecks;
        return PAM_AUTH_ERR;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->m_passwordChangeToken = QStringLiteral("bad-password");
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !worker->m_passwordChange->isActive(); }, 3000));
    EXPECT_EQ(pamChecks, 1);
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(finished.count(), 0);

    // An interrupted password change still requires a new credential check.
    model->setLockPasswordChangeRequiresAuthentication(true);
    worker->authFinishedAction();
    EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(finished.count(), 0);
    worker->cancelPasswordChange();

    model->setLockPasswordChangeRequiresAuthentication(false);
    user->m_isNoPasswordLogin = true;
    worker->checkAccount(user->name(), false);
    EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_EQ(service.requests, 0);
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordGate, CancelRejectsLateAccountInfoSuccess)
{
    AccountExpiryTestService service;
    ASSERT_TRUE(service.registerOnBus());
    service.deferReply = true;
    ASSERT_TRUE(model->updateCurrentUser(std::make_shared<AccountInfoLockTestUser>()));
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !service.pending.isEmpty(); }, 1000));
    QPointer<QThread> thread = worker->m_passwordChange->m_checkThread;
    worker->cancelPasswordChange();
    service.answerPending();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !thread || !thread->isRunning(); }, 3000));
    QTest::qWait(20);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(worker->m_passwordChangePassed);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, NavigatingAwayCancelsWithoutRestartingAuthentication)
{
    AuthenticationLookupTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &service));
    QTest::qWait(20);
    worker->createAuthentication(model->currentUser()->name());
    ASSERT_EQ(service.lookups, 1);

    auto content = managedContent();
    QObject::connect(content.get(), &LockContent::requestEndAuthentication, worker, &LockWorker::onEndAuthentication);
    const auto factors = AUTH_FLAGS_CAST(AT_Password | AT_Fingerprint);
    model->setAuthType(factors);
    model->updateMFAFlag(true);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    for (auto mode : {SessionBaseModel::UserMode, SessionBaseModel::PowerMode}) {
        SCOPED_TRACE(static_cast<int>(mode));
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
        worker->m_passwordChangeToken = QStringLiteral("old");
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
        service.lookups = 0;
        QSignalSpy ended(content.get(), &LockContent::requestEndAuthentication);
        if (mode == SessionBaseModel::UserMode)
            emit content->m_controlWidget->requestSwitchUser();
        else
            emit content->m_controlWidget->requestShutdown();
        EXPECT_EQ(ended.count(), mode == SessionBaseModel::UserMode ? 1 : 0);
        EXPECT_FALSE(worker->m_passwordChange->isActive());
        EXPECT_FALSE(model->lockPasswordChangeActive());
        QTest::qWait(30);
        EXPECT_EQ(service.lookups, 0);
        EXPECT_EQ(model->currentModeState(), mode);
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(model->getAuthProperty().AuthType, factors);
        EXPECT_EQ(finished.count(), 0);
    }
}

TEST_F(UT_LockPasswordGate, CancelAndIdleTimeoutRestartOnVisiblePasswordAndConfirmationPages)
{
    AuthenticationLookupTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &service));
    QTest::qWait(20);
    for (auto mode : {SessionBaseModel::PasswordMode, SessionBaseModel::ConfirmPasswordMode}) {
        SCOPED_TRACE(static_cast<int>(mode));
        for (bool timeout : {false, true}) {
            SCOPED_TRACE(timeout);
            model->setCurrentModeState(mode);
            worker->authFinishedAction();
            ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
            ASSERT_TRUE(model->visible());
            service.lookups = 0;
            if (timeout)
                worker->m_passwordChange->m_timer.start(1);
            else
                worker->cancelPasswordChange();
            EXPECT_EQ(service.lookups, 0);
            ASSERT_TRUE(QTest::qWaitFor([&] { return service.lookups == 1; }, 1000));
            EXPECT_EQ(model->currentModeState(), mode);
            EXPECT_FALSE(worker->m_passwordChange->isActive());
        }
    }
}

TEST_F(UT_LockPasswordGate, QueuedCancelDoesNotRestartAfterLeavingOrHidingAuthenticationPage)
{
    AuthenticationLookupTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &service));
    QTest::qWait(20);
    enum Destination { UserPage, PowerPage, Hidden, BlackScreen };
    for (auto destination : {UserPage, PowerPage, Hidden, BlackScreen}) {
        SCOPED_TRACE(static_cast<int>(destination));
        model->setIsBlackMode(false);
        model->setCurrentModeState(SessionBaseModel::PasswordMode);
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
        service.lookups = 0;
        worker->cancelPasswordChange();
        EXPECT_EQ(service.lookups, 0);
        // Change the destination after cancellation, before its queued recovery.
        if (destination == UserPage)
            model->setCurrentModeState(SessionBaseModel::UserMode);
        else if (destination == PowerPage)
            model->setCurrentModeState(SessionBaseModel::PowerMode);
        else if (destination == Hidden)
            model->setVisible(false);
        else
            model->setIsBlackMode(true);
        QTest::qWait(30);
        EXPECT_EQ(service.lookups, 0);
        EXPECT_FALSE(worker->m_passwordChange->isActive());
    }
}

TEST_F(UT_LockPasswordGate, DisabledPreservesNormalCompletion)
{
    worker->m_forcePasswordChange = false;
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    EXPECT_EQ(finished.count(), 1);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, PasswordlessAccountChecksEmptyTokenBeforeUnlock)
{
    model->currentUser()->m_isPasswordValid = false;
    worker->m_passwordChange->m_checker = [](const QString &, const QString &password) {
        return password.isEmpty() ? PAM_SUCCESS : PAM_AUTH_ERR;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->checkAccount(model->currentUser()->name(), false);
    EXPECT_EQ(finished.count(), 0);
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, NoPasswordLoginWithStoredPasswordStillRequiresCredential)
{
    model->currentUser()->m_isNoPasswordLogin = true;
    model->currentUser()->m_isPasswordValid = true;
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy forms(model, &SessionBaseModel::lockPasswordChangeForm);
    worker->checkAccount(model->currentUser()->name(), false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !forms.isEmpty(); }, 3000));
    EXPECT_TRUE(forms.last()[0].toBool());
    EXPECT_TRUE(forms.last()[1].toBool());
    worker->submitPasswordChange({}, {}, {}, {});
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->acceptsInput());
}

TEST_F(UT_LockPasswordGate, PasswordlessPamFailureKeepsSessionLocked)
{
    model->currentUser()->m_isPasswordValid = false;
    worker->m_passwordChange->m_checker = [](const QString &, const QString &) { return PAM_AUTH_ERR; };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy canceled(worker->m_passwordChange, &LockPasswordChange::canceled);
    worker->checkAccount(model->currentUser()->name(), false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canceled.count() == 1; }, 3000));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(worker->m_passwordChangePassed);
}

TEST_F(UT_LockPasswordGate, PasswordlessExpiryFormDoesNotRequireOldPassword)
{
    model->currentUser()->m_isPasswordValid = false;
    auto content = managedContent();
    worker->checkAccount(model->currentUser()->name(), false);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return model->lockPasswordChangePageVisible() && worker->m_passwordChange->acceptsInput();
    }, 3000));
    auto *page = content->m_passwordChangeWidget;
    ASSERT_NE(page, nullptr);
    EXPECT_FALSE(page->m_currentRequired);
    EXPECT_TRUE(page->m_current->text().isEmpty());
    page->m_localSecurityCheck = false;
    page->m_password->setText(QStringLiteral("newSecret!"));
    page->m_confirmation->setText(QStringLiteral("newSecret!"));
    EXPECT_TRUE(page->validateInputs());
    worker->cancelPasswordChange();
}

TEST_F(UT_LockPasswordGate, LoginNeverUsesLockPasswordGate)
{
    model->setAppType(AuthCommon::Login);
    EXPECT_FALSE(worker->passwordChangeEnabled());
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    EXPECT_EQ(finished.count(), 1);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, OrdinaryCompletionWaitsForPasswordChange)
{
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->isActive());
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    EXPECT_TRUE(model->visible());
}

TEST_F(UT_LockPasswordGate, MfaCompletionCannotBypassPasswordChange)
{
    model->updateMFAFlag(true);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->onAuthStateChanged(AT_All, AS_Success, QStringLiteral("Success"));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, LockServiceCompletionCannotBypassPasswordChange)
{
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->lockServiceEvent(DBusLockService::Success, 0, model->currentUser()->name(), QStringLiteral("Success"));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, SessionManagerAndSetLockedCannotBypassPasswordChange)
{
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    emit worker->m_sessionManagerInter->Unlock();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(worker->m_passwordChange->isActive());
    worker->setLocked(false);
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordGate, PasswordChangeKeepsLockScreenVisible)
{
    model->setVisible(true);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    model->setVisible(false);
    EXPECT_TRUE(model->visible());
}

TEST_F(UT_LockPasswordGate, KeyboardAndMouseActivityRenewPasswordInputIdleDeadline)
{
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    auto *change = worker->m_passwordChange;
    QSignalSpy canceled(change, &LockPasswordChange::canceled);
    AppEventFilter activity;
    QObject::connect(&activity, &AppEventFilter::userIsActive, worker, &LockWorker::restartResetSessionTimer);
    QWidget input;
    input.installEventFilter(&activity);

    for (auto stage : {LockPasswordChange::CurrentPassword, LockPasswordChange::NewPassword, LockPasswordChange::ConfirmPassword}) {
        ASSERT_EQ(change->m_stage, stage);
        change->m_timer.start(50);
        QTest::keyClick(&input, Qt::Key_A);
        EXPECT_GT(change->m_timer.remainingTime(), 50000);
        change->m_timer.start(50);
        QTest::mouseClick(&input, Qt::LeftButton);
        EXPECT_GT(change->m_timer.remainingTime(), 50000);
        QTest::qWait(70);
        EXPECT_EQ(canceled.count(), 0);
        EXPECT_FALSE(worker->m_resetSessionTimer->isActive());
        if (stage == LockPasswordChange::CurrentPassword) {
            change->submit(QStringLiteral("old"));
            ASSERT_TRUE(QTest::qWaitFor([&] { return change->m_stage == LockPasswordChange::NewPassword; }, 3000));
        } else if (stage == LockPasswordChange::NewPassword) {
            change->submit(QStringLiteral("new"));
        }
    }
    // With no further activity the idle deadline still cancels the flow.
    change->m_timer.start(1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canceled.count() == 1; }, 3000));
    EXPECT_FALSE(change->isActive());
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
    EXPECT_TRUE(model->visible());
    EXPECT_TRUE(change->m_oldPassword.isEmpty());
    EXPECT_TRUE(change->m_newPassword.isEmpty());
}

TEST_F(UT_LockPasswordGate, UserActivityDoesNotExtendExpiryCheckTimeout)
{
    worker->m_passwordChangeToken = QStringLiteral("old");
    worker->m_passwordChange->m_checker = [](const QString &, const QString &) {
        QThread::msleep(150);
        return PAM_SUCCESS;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy canceled(worker->m_passwordChange, &LockPasswordChange::canceled);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangeActive(); }, 3000));
    ASSERT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CheckingExpiry);
    worker->m_passwordChange->m_timer.start(1);
    worker->restartResetSessionTimer();
    EXPECT_EQ(worker->m_passwordChange->m_timer.interval(), 1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canceled.count() == 1; }, 3000));
    QTest::qWait(200);
    EXPECT_EQ(finished.count(), 0);
}

TEST_F(UT_LockPasswordGate, SuspendRecoveryWithoutSleepLockKeepsAllPasswordFramesAccessible)
{
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    QWidget firstFrame, secondFrame;
    for (auto *frame : {&firstFrame, &secondFrame}) {
        frame->setVisible(model->visible());
        QObject::connect(model, &SessionBaseModel::visibleChanged, frame, &QWidget::setVisible);
    }
    DBusLockAgent agent;
    agent.setModel(model);
    agent.Suspend(true);
    QSignalSpy visibility(model, &SessionBaseModel::visibleChanged);
    // The private bus has no power service, so SleepLock reads as false.
    agent.Suspend(false);
    agent.Show();
    EXPECT_TRUE(model->visible());
    EXPECT_TRUE(firstFrame.isVisible());
    EXPECT_TRUE(secondFrame.isVisible());
    EXPECT_FALSE(model->isBlackMode());
    for (const auto &signal : visibility) {
        EXPECT_TRUE(signal[0].toBool());
    }
}

TEST_F(UT_LockPasswordGate, PasswordlessSuspendCancellationKeepsLockUntilFreshCheck)
{
    SleepLockTestService power;
    auto bus = QDBusConnection::sessionBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::sessionPowerService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::sessionPowerPath, &power));
    model->currentUser()->m_isPasswordValid = false;
    DBusLockAgent agent;
    agent.setModel(model);
    for (bool earlySleepNotification : {false, true}) {
        SCOPED_TRACE(earlySleepNotification);
        worker->m_passwordChangePassed = false;
        worker->m_passwordChange->m_checker = [](const QString &, const QString &) { return PAM_NEW_AUTHTOK_REQD; };
        worker->checkAccount(model->currentUser()->name(), false);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return model->lockPasswordChangePageVisible() && worker->m_passwordChange->acceptsInput();
        }, 3000));
        QSignalSpy finished(model, &SessionBaseModel::authFinished);
        // login1 may announce sleep before the lock agent. Protection must
        // survive cancellation before either agent callback is entered.
        if (earlySleepNotification)
            emit model->prepareForSleep(true);
        agent.Suspend(true);
        EXPECT_FALSE(worker->m_passwordChange->isActive());
        QCoreApplication::processEvents();
        agent.Suspend(false);
        EXPECT_TRUE(model->visible());
        EXPECT_FALSE(model->isBlackMode());
        EXPECT_TRUE(model->lockPasswordChangeProtected());
        EXPECT_FALSE(worker->m_passwordChangePassed);
        EXPECT_EQ(finished.count(), 0);
        // Repeated or late visibility callbacks must not release the lock.
        agent.Suspend(false);
        model->setVisible(false);
        EXPECT_TRUE(model->visible());

        int checks = 0;
        worker->m_passwordChange->m_checker = [&checks](const QString &, const QString &password) {
            ++checks;
            return password.isEmpty() ? PAM_SUCCESS : PAM_AUTH_ERR;
        };
        worker->checkAccount(model->currentUser()->name(), false);
        ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
        EXPECT_EQ(checks, 1);
        EXPECT_TRUE(finished[0][0].toBool());
        EXPECT_FALSE(model->lockPasswordChangeProtected());
        EXPECT_FALSE(model->visible());
    }
}

TEST_F(UT_LockPasswordGate, OrdinarySuspendStillHonorsDisabledSleepLock)
{
    SleepLockTestService power;
    auto bus = QDBusConnection::sessionBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::sessionPowerService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::sessionPowerPath, &power));
    model->setVisible(true);
    DBusLockAgent agent;
    agent.setModel(model);
    agent.Suspend(true);
    agent.Suspend(false);
    EXPECT_FALSE(model->visible());
    EXPECT_FALSE(model->lockPasswordChangeProtected());
}

TEST_F(UT_LockPasswordGate, SleepRejectsLatePamSuccessBeforeAndAfterRecovery)
{
    for (bool hibernate : {false, true}) {
        for (bool replyBeforeRecovery : {false, true}) {
            auto release = std::make_shared<QSemaphore>();
            auto entered = std::make_shared<QSemaphore>();
            worker->m_passwordChangePassed = false;
            worker->m_passwordChangeToken = QStringLiteral("cached-old");
            worker->m_passwordChange->m_checker = [release, entered](const QString &, const QString &password) {
                if (password == QStringLiteral("cached-old")) {
                    entered->release();
                    release->acquire();
                }
                return PAM_SUCCESS;
            };
            const auto unblockOnExit = qScopeGuard([release] { release->release(); });
            QSignalSpy finished(model, &SessionBaseModel::authFinished);
            worker->authFinishedAction();
            ASSERT_TRUE(QTest::qWaitFor([&] {
                return model->lockPasswordChangeActive() && entered->tryAcquire();
            }, 3000));
            ASSERT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CheckingExpiry);
            QPointer<QThread> pending = worker->m_passwordChange->m_checkThread;
            DBusLockAgent agent;
            agent.setModel(model);
            const auto sleep = [&](bool enable) {
                if (hibernate) {
                    agent.Hibernate(enable);
                } else {
                    agent.Suspend(enable);
                }
            };
            worker->m_passwordChangeHint = QStringLiteral("discarded hint");
            sleep(true);
            EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
            EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
            EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
            EXPECT_TRUE(worker->m_passwordChangeHint.isEmpty());
            if (!replyBeforeRecovery) {
                sleep(false);
            }
            release->release();
            ASSERT_TRUE(QTest::qWaitFor([&] { return !pending || !pending->isRunning(); }, 3000));
            QTest::qWait(20);
            EXPECT_EQ(finished.count(), 0);
            EXPECT_FALSE(worker->m_passwordChangePassed);
            if (replyBeforeRecovery) {
                sleep(false);
            }
            EXPECT_TRUE(model->visible());
            EXPECT_TRUE(model->lockPasswordChangeActive());
            EXPECT_EQ(finished.count(), 0);
            worker->submitPasswordChange(QStringLiteral("fresh-current"), {}, {}, {});
            ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
            EXPECT_TRUE(finished[0][0].toBool());
            EXPECT_TRUE(worker->m_passwordChangePassed);
        }
    }
}

TEST_F(UT_LockPasswordGate, SuspendRecoveryReenablesRealFramePowerKeyWithoutLogindNotification)
{
    QWidget placeholder;
    QScopedValueRollback<QPointer<QWidget>> content(FullScreenBackground::currentContent, &placeholder);
    LockFrame frame(model);
    // Let the existing startup guard expire before testing the suspend guard.
    ASSERT_TRUE(QTest::qWaitFor([&] { return frame.m_enablePowerOffKey; }, 2000));
    DBusLockAgent agent;
    agent.setModel(model);
    for (bool enabled : {false, true}) {
        worker->m_forcePasswordChange = enabled;
        if (enabled) {
            worker->authFinishedAction();
            ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
        }
        QSignalSpy sleep(model, &SessionBaseModel::prepareForSleep);
        agent.Suspend(true);
        ASSERT_EQ(sleep.count(), 1);
        EXPECT_TRUE(sleep[0][0].toBool());
        EXPECT_FALSE(frame.m_enablePowerOffKey);
        agent.Suspend(false);
        ASSERT_EQ(sleep.count(), 2);
        EXPECT_FALSE(sleep[1][0].toBool());
        EXPECT_FALSE(frame.m_enablePowerOffKey);
        // No logind PrepareForSleep(false) is sent: the agent itself must
        // schedule recovery of the actual LockFrame's power-key permission.
        ASSERT_TRUE(QTest::qWaitFor([&] { return frame.m_enablePowerOffKey; }, 4000));
        if (enabled) {
            worker->cancelPasswordChange();
        }
    }
}

TEST_F(UT_LockPasswordGate, DismissingShutdownOnUnlockedDesktopDoesNotStartPasswordCheck)
{
    LockContent content;
    content.m_model = model;
    model->setCurrentModeState(SessionBaseModel::ShutDownMode);
    model->setVisible(true);
    QTest::keyClick(&content, Qt::Key_Escape);
    QTest::qWait(50);
    EXPECT_EQ(model->currentModeState(), SessionBaseModel::PasswordMode);
    EXPECT_FALSE(model->visible());
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(worker->m_passwordChangePassed);
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
}

TEST_F(UT_LockPasswordGate, ManagedPageExposesCurrentAndNewPasswordAndRestoresAfterCancel)
{
    for (bool expired : {false, true}) {
        auto content = managedContent();
        auto *pluginPage = content->m_pluginWidgets.value(SessionBaseWindow::FullManagedPlugin);
        ASSERT_EQ(content->m_stackedLayout->currentWidget(), pluginPage);
        model->currentUser()->updatePasswordExpiredState(expired ? User::ExpiredAlready : User::ExpiredNormal, 0);
        worker->m_passwordChangeToken = expired ? QStringLiteral("test-old-password") : QString();
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return model->lockPasswordChangeActive() && content->m_passwordChangeWidget
                && !content->m_passwordChangeWidget->m_busy
                && content->m_passwordChangeWidget->m_checkingOnly == !expired;
        }, 3000));
        auto *page = content->m_passwordChangeWidget;
        EXPECT_EQ(content->m_stackedLayout->currentIndex(), 0);
        EXPECT_EQ(content->m_centerWidget, page);
        EXPECT_EQ(page->m_current->isHidden(), expired);
        EXPECT_EQ(page->m_password->isHidden(), !expired);
        EXPECT_EQ(page->m_confirmation->isHidden(), !expired);
        EXPECT_EQ(page->m_hint->isHidden(), !expired);
        EXPECT_TRUE(page->m_submit->isEnabled());
        content->pushPasswordFrame();
        EXPECT_EQ(content->m_stackedLayout->currentIndex(), 0);
        worker->cancelPasswordChange();
        EXPECT_EQ(content->m_stackedLayout->currentWidget(), pluginPage);
        EXPECT_FALSE(content->m_centerTopFrame->isHidden());
        QTest::qWait(30);
    }
}

TEST_F(UT_LockPasswordGate, ManagedPageRestoresAfterPasswordChangeSuccess)
{
    auto content = managedContent();
    auto *pluginPage = content->m_pluginWidgets.value(SessionBaseWindow::FullManagedPlugin);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    EXPECT_EQ(content->m_stackedLayout->currentIndex(), 0);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->m_passwordChange->finish(true);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_EQ(content->m_stackedLayout->currentWidget(), pluginPage);
    EXPECT_FALSE(model->visible());
}

TEST_F(UT_LockPasswordGate, ManagedPluginTokensCannotEnterTheTemporaryPamInput)
{
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    ASSERT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    worker->sendTokenToAuth(model->currentUser()->name(), AT_Custom, QStringLiteral("plugin-token"));
    EXPECT_EQ(worker->m_passwordChange->m_stage, LockPasswordChange::CurrentPassword);
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
}

TEST_F(UT_LockPasswordGate, PasswordChangePreservesPendingPowerConfirmation)
{
    model->setCurrentModeState(SessionBaseModel::ConfirmPasswordMode);
    model->m_powerAction = SessionBaseModel::RequireRestart;
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    EXPECT_EQ(model->currentModeState(), SessionBaseModel::ConfirmPasswordMode);
    EXPECT_EQ(model->powerAction(), SessionBaseModel::RequireRestart);
}

TEST_F(UT_LockPasswordGate, DisabledDoesNotCachePasswordToken)
{
    worker->m_forcePasswordChange = false;
    worker->sendTokenToAuth(model->currentUser()->name(), AT_Password, QStringLiteral("test-password"));
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, UnexpiredAccountCompletesAfterAccountCheck)
{
    worker->m_passwordChangeToken = QStringLiteral("test-old-password");
    worker->m_passwordChange->m_checker = [](const QString &, const QString &) { return PAM_SUCCESS; };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    EXPECT_EQ(finished.count(), 0);
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(worker->m_passwordChangePassed);
}

TEST_F(UT_LockPasswordGate, FastSuccessfulCheckNeverPresentsIntermediatePage)
{
    auto content = managedContent();
    auto *originalPage = content->m_stackedLayout->currentWidget();
    auto release = std::make_shared<QSemaphore>();
    auto entered = std::make_shared<QSemaphore>();
    const auto unblockOnExit = qScopeGuard([release] { release->release(); });
    model->setVisible(true);
    worker->m_passwordChangeToken = QStringLiteral("old");
    worker->m_passwordChange->m_checker = [release, entered](const QString &, const QString &) {
        entered->release();
        release->acquire();
        return PAM_SUCCESS;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy forms(model, &SessionBaseModel::lockPasswordChangeForm);
    QSignalSpy prompts(model, &SessionBaseModel::lockPasswordChangePrompt);
    worker->authFinishedAction();
    EXPECT_TRUE(model->lockPasswordChangeActive());
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
    ASSERT_TRUE(QTest::qWaitFor([&] { return entered->tryAcquire(); }, 3000));
    // Visibility cleanup, repeated completion and layout refresh must not bypass
    // the pending check or expose its temporary form.
    model->setVisible(false);
    worker->authFinishedAction();
    worker->sendTokenToAuth(model->currentUser()->name(), AT_PAM, QStringLiteral("ignored"));
    content->pushPasswordFrame();
    EXPECT_TRUE(model->visible());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(content->m_stackedLayout->currentWidget(), originalPage);
    EXPECT_EQ(content->m_passwordChangeWidget, nullptr);
    release->release();
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(forms.count(), 0);
    EXPECT_EQ(prompts.count(), 0);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    QTest::qWait(300);
    EXPECT_EQ(content->m_passwordChangeWidget, nullptr);
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
}

TEST_F(UT_LockPasswordGate, SlowCheckPresentsBusyPageAndCancelRejectsLateSuccess)
{
    auto content = managedContent();
    auto release = std::make_shared<QSemaphore>();
    const auto unblockOnExit = qScopeGuard([release] { release->release(); });
    worker->m_passwordChangeToken = QStringLiteral("old");
    worker->m_passwordChange->m_checker = [release](const QString &, const QString &) {
        release->acquire();
        return PAM_SUCCESS;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->m_passwordChangeWidget; }, 3000));
    EXPECT_TRUE(model->lockPasswordChangePageVisible());
    EXPECT_TRUE(content->m_passwordChangeWidget->m_busy);
    EXPECT_TRUE(content->m_passwordChangeWidget->m_checkingOnly);
    worker->cancelPasswordChange();
    release->release();
    QTest::qWait(300);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
}

TEST_F(UT_LockPasswordGate, KnownExpiredAccountShowsInputWithoutIntermediateCheckingPage)
{
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("old");
    QSignalSpy forms(model, &SessionBaseModel::lockPasswordChangeForm);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->m_passwordChangeWidget; }, 3000));
    ASSERT_GT(forms.count(), 0);
    for (const auto &form : forms) {
        EXPECT_FALSE(form[1].toBool());
        EXPECT_FALSE(form[2].toBool());
    }
    EXPECT_TRUE(content->m_passwordChangeWidget->m_password->isEnabled());
}

TEST_F(UT_LockPasswordGate, CancelCannotFinishUnlock)
{
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    worker->cancelPasswordChange();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(worker->m_passwordChangePassed);
}

TEST_F(UT_LockPasswordGate, FreshCurrentPasswordCheckClearsStaleExpiredCache)
{
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("cached-old");
    worker->m_passwordChange->m_checker = [](const QString &, const QString &password) {
        return password == QStringLiteral("new-current") ? PAM_SUCCESS : PAM_AUTH_ERR;
    };
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return model->lockPasswordChangePageVisible() && worker->m_passwordChange->m_stage == LockPasswordChange::NewPassword;
    }, 3000));
    worker->m_passwordChange->restartAtCurrentPassword();
    worker->submitPasswordChange(QStringLiteral("new-current"), {}, {}, {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 3000));
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(model->currentUser()->expiredState(), User::ExpiredNormal);
    EXPECT_EQ(worker->m_passwordChange->m_process, nullptr);
    EXPECT_TRUE(worker->m_passwordChangeHint.isEmpty());
}

TEST_F(UT_LockPasswordGate, ChangedPasswordRestoresAuthenticationAndCompletesOnce)
{
    model->setAuthType(AT_Password);
    model->updateMFAFlag(true);
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("test-old-password");
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_FALSE(model->getAuthProperty().MFAFlag);
    // Simulate a successful passwd result; controller process I/O is tested separately.
    worker->m_passwordChange->finish(true);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_EQ(model->currentUser()->expiredState(), User::ExpiredNormal);
    EXPECT_TRUE(model->getAuthProperty().MFAFlag);
    EXPECT_EQ(model->getAuthProperty().AuthType, AT_Password);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    EXPECT_TRUE(worker->m_passwordChange->m_newPassword.isEmpty());
}

TEST_F(UT_LockPasswordGate, LateFrameworkFactorsCannotReplacePasswordInput)
{
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    emit worker->m_authFramework->MFAFlagChanged(true);
    emit worker->m_authFramework->FactorsInfoChanged({});
    EXPECT_FALSE(model->getAuthProperty().MFAFlag);
    EXPECT_EQ(model->getAuthProperty().AuthType, AT_PAM);
}

TEST_F(UT_LockPasswordGate, DelayedLockedStateCannotOverwritePasswordPrompt)
{
    model->updateMFAFlag(true);
    worker->onAuthStateChanged(AT_Password, AS_Locked, QStringLiteral("old-auth-state"));
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    QTest::qWait(100);
    EXPECT_EQ(model->getAuthResult().authState, AS_Cancel);
}

TEST_F(UT_LockPasswordGate, AnotherUidCannotEnterPasswordChange)
{
    auto other = std::make_shared<ValidLockTestUser>();
    other->m_uid = getuid() + 1;
    model->updateCurrentUser(other);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy errors(model, &SessionBaseModel::lockPasswordChangePrompt);
    worker->authFinishedAction();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(errors.count(), 1);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
}

TEST_F(UT_LockPasswordGate, SwitchingUserDiscardsPendingExpiryResult)
{
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->m_passwordChangeToken = QStringLiteral("test-old-password");
    worker->m_passwordChange->m_checker = [](const QString &, const QString &) {
        QThread::msleep(50);
        return PAM_SUCCESS;
    };
    worker->authFinishedAction();
    auto other = std::make_shared<ValidLockTestUser>();
    other->m_name = QStringLiteral("other-test-user");
    model->updateCurrentUser(other);
    QTest::qWait(100);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_FALSE(model->lockPasswordChangeActive());
}

TEST_F(UT_LockPasswordGate, OriginalInputIsReenabledForNewPasswordAndConfirmation)
{
    model->setAuthType(AT_PAM);
    SFAWidget widget;
    widget.setModel(model);
    ASSERT_NE(widget.m_singleAuth, nullptr);
    widget.m_singleAuth->setPasswordLineEditEnabled(false);
    model->setLockPasswordChangeActive(true);
    emit model->lockPasswordChangePrompt(QStringLiteral("Please enter the new password."), {}, false);
    EXPECT_TRUE(widget.m_singleAuth->passwordLineEditEnabled());
    EXPECT_FALSE(widget.m_singleAuth->m_lineEdit->lineEdit()->isReadOnly());
    EXPECT_EQ(widget.m_singleAuth->m_lineEdit->lineEdit()->validator(), nullptr);
    EXPECT_EQ(widget.m_singleAuth->m_lineEdit->lineEdit()->placeholderText(), QStringLiteral("Please enter the new password."));
    widget.m_singleAuth->m_lineEdit->lineEdit()->insert(QStringLiteral("New password ~!"));
    QSignalSpy tokens(&widget, &SFAWidget::sendTokenToAuth);
    QTest::keyClick(widget.m_singleAuth->m_lineEdit->lineEdit(), Qt::Key_Return);
    ASSERT_EQ(tokens.count(), 1);
    EXPECT_EQ(tokens[0][2].toString(), QStringLiteral("New password ~!"));
    emit model->lockPasswordChangePrompt(QStringLiteral("Confirm the new password."), {}, false);
    EXPECT_TRUE(widget.m_singleAuth->lineEditText().isEmpty());
    EXPECT_FALSE(widget.m_singleAuth->m_lineEdit->lineEdit()->isReadOnly());
    emit model->lockPasswordChangePrompt(QStringLiteral("Changing password..."), {}, true);
    EXPECT_FALSE(widget.m_singleAuth->passwordLineEditEnabled());
    EXPECT_TRUE(widget.m_singleAuth->m_lineEdit->lineEdit()->isReadOnly());
    model->setLockPasswordChangeActive(false);
    emit model->lockPasswordChangePrompt({}, {}, false);
    EXPECT_TRUE(widget.m_singleAuth->passwordLineEditEnabled());
    EXPECT_FALSE(widget.m_singleAuth->m_lineEdit->lineEdit()->isReadOnly());
    EXPECT_TRUE(widget.m_singleAuth->lineEditText().isEmpty());
    EXPECT_EQ(widget.m_singleAuth->m_lineEdit->lineEdit()->validator(), widget.m_singleAuth->m_passwordValidator);
}

TEST_F(UT_LockPasswordGate, AccountRefreshPreservesPasswordChangeInputPermissions)
{
    model->setAuthType(AT_PAM);
    SFAWidget widget;
    widget.setModel(model);
    ASSERT_NE(widget.m_singleAuth, nullptr);
    auto *input = widget.m_singleAuth->m_lineEdit->lineEdit();
    model->setLockPasswordChangeActive(true);
    for (const auto &prompt : {QStringLiteral("Checking password expiration..."), QStringLiteral("Changing password...")}) {
        emit model->lockPasswordChangePrompt(prompt, {}, true);
        for (auto state : {User::ExpiredNormal, User::ExpiredSoon, User::ExpiredAlready}) {
            model->currentUser()->updatePasswordExpiredState(state, 1);
            emit model->currentUser()->passwordExpiredInfoChanged();
            // The hourly refresh calls the same slot directly.
            widget.updatePasswordExpiredState();
            EXPECT_FALSE(widget.m_singleAuth->passwordLineEditEnabled());
            EXPECT_TRUE(input->isReadOnly());
            EXPECT_FALSE(input->isEnabled());
            EXPECT_EQ(input->placeholderText(), prompt);
        }
    }
    emit model->lockPasswordChangePrompt(QStringLiteral("Please enter the new password."), {}, false);
    widget.updatePasswordExpiredState();
    EXPECT_TRUE(widget.m_singleAuth->passwordLineEditEnabled());
    EXPECT_FALSE(input->isReadOnly());
    model->setLockPasswordChangeActive(false);
    emit model->lockPasswordChangePrompt({}, {}, false);
    widget.m_singleAuth->setPasswordLineEditEnabled(false);
    widget.updatePasswordExpiredState();
    EXPECT_TRUE(widget.m_singleAuth->passwordLineEditEnabled());
    EXPECT_EQ(input->validator(), widget.m_singleAuth->m_passwordValidator);
}

TEST_F(UT_LockPasswordGate, CancelRestoresTheSamePamInput)
{
    model->setAuthType(AT_PAM);
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    SFAWidget widget;
    widget.setModel(model);
    auto *input = widget.m_singleAuth.data();
    ASSERT_NE(input, nullptr);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    input->setPasswordLineEditEnabled(false);
    worker->cancelPasswordChange();
    EXPECT_EQ(finished.count(), 0);
    ASSERT_EQ(widget.m_singleAuth, input);
    EXPECT_TRUE(input->passwordLineEditEnabled());
    EXPECT_FALSE(input->m_lineEdit->lineEdit()->isReadOnly());
    EXPECT_EQ(input->m_lineEdit->lineEdit()->validator(), input->m_passwordValidator);
    EXPECT_TRUE(input->lineEditText().isEmpty());
}

TEST_F(UT_LockPasswordGate, LoginInputIgnoresLockPasswordPrompts)
{
    model->setAppType(AuthCommon::Login);
    model->setAuthType(AT_PAM);
    SFAWidget widget;
    widget.setModel(model);
    ASSERT_NE(widget.m_singleAuth, nullptr);
    widget.m_singleAuth->setPasswordLineEditEnabled(false);
    model->setLockPasswordChangeActive(true);
    emit model->lockPasswordChangePrompt(QStringLiteral("Please enter the new password."), {}, false);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(widget.m_singleAuth->passwordLineEditEnabled());
}

TEST_F(UT_LockPasswordGate, ExpiredBiometricFormShowsAllFieldsAndClearsOnCancel)
{
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    auto *page = content->m_passwordChangeWidget;
    EXPECT_FALSE(page->m_current->isHidden());
    EXPECT_FALSE(page->m_password->isHidden());
    EXPECT_FALSE(page->m_confirmation->isHidden());
    EXPECT_FALSE(page->m_hint->isHidden());
    content->show();
    content->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(content.get(), 1000));
    page->m_current->setText(QStringLiteral("old"));
    page->m_password->setText(QStringLiteral("new"));
    page->m_confirmation->setText(QStringLiteral("different"));
    page->m_hint->setText(QStringLiteral("a hint"));
    QTest::mouseClick(page->m_submit, Qt::LeftButton);
    EXPECT_TRUE(page->m_confirmation->isAlert());
    EXPECT_TRUE(page->m_confirmation->lineEdit()->hasFocus());
    EXPECT_EQ(worker->m_passwordChange->m_process, nullptr);
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    EXPECT_EQ(page->m_current->text(), QStringLiteral("old"));
    EXPECT_EQ(page->m_password->text(), QStringLiteral("new"));
    EXPECT_EQ(page->m_confirmation->text(), QStringLiteral("different"));
    EXPECT_EQ(page->m_hint->text(), QStringLiteral("a hint"));
    content->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(content.get(), 1000));
    page->m_confirmation->lineEdit()->setFocus();
    ASSERT_TRUE(page->m_confirmation->lineEdit()->hasFocus());
    worker->m_passwordChange->refreshPrompt();
    EXPECT_EQ(page->m_password->text(), QStringLiteral("new"));
    EXPECT_EQ(page->m_confirmation->text(), QStringLiteral("different"));
    EXPECT_TRUE(page->m_confirmation->lineEdit()->hasFocus());
    // A raw string containing HTML is rendered as plain text.
    emit model->lockPasswordChangeForm(true, false, false, QStringLiteral("<b>policy error</b>"));
    EXPECT_EQ(page->m_error->toPlainText(), QStringLiteral("<b>policy error</b>"));
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QTest::keyClick(page->m_password->lineEdit(), Qt::Key_Escape);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_TRUE(page->m_current->text().isEmpty());
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_TRUE(page->m_confirmation->text().isEmpty());
    EXPECT_TRUE(page->m_hint->text().isEmpty());
}

TEST_F(UT_LockPasswordGate, BusyFormIgnoresAccountRefreshAndRepeatedSubmissions)
{
    auto content = managedContent();
    worker->m_passwordChangeToken = QStringLiteral("old");
    worker->m_passwordChange->m_checker = [](const QString &, const QString &) {
        QThread::msleep(600);
        return PAM_NEW_AUTHTOK_REQD;
    };
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->m_passwordChangeWidget; }, 3000));
    auto *page = content->m_passwordChangeWidget;
    ASSERT_TRUE(page->m_busy);
    for (auto state : {User::ExpiredNormal, User::ExpiredSoon, User::ExpiredAlready}) {
        model->currentUser()->updatePasswordExpiredState(state, 1);
        emit model->currentUser()->passwordExpiredInfoChanged();
        content->m_authWidget->updatePasswordExpiredState();
        EXPECT_FALSE(page->m_current->isEnabled());
        EXPECT_FALSE(page->m_password->isEnabled());
        EXPECT_FALSE(page->m_confirmation->isEnabled());
        EXPECT_FALSE(page->m_hint->isEnabled());
        EXPECT_FALSE(page->m_submit->isEnabled());
    }
    worker->m_passwordChangeHint = QStringLiteral("original hint");
    worker->submitPasswordChange(QStringLiteral("old"), QStringLiteral("new"), QStringLiteral("new"), QStringLiteral("ignored hint"));
    EXPECT_EQ(worker->m_passwordChangeHint, QStringLiteral("original hint"));
    EXPECT_EQ(worker->m_passwordChange->m_process, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !page->m_busy; }, 3000));
    EXPECT_TRUE(page->m_password->isEnabled());
}

TEST_F(UT_LockPasswordGate, FullFormAndLongErrorFit1024x768)
{
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    content->resize(1024, 768);
    content->show();
    auto *page = content->m_passwordChangeWidget;
    emit model->lockPasswordChangeForm(true, false, false, QStringLiteral("<b>domain policy error</b>\n").repeated(80));
    QTest::qWait(20);
    EXPECT_LE(content->height(), 768);
    for (auto *widget : QList<QWidget *> {page->m_current, page->m_password, page->m_confirmation, page->m_hint, page->m_submit}) {
        EXPECT_TRUE(widget->isVisible());
        EXPECT_TRUE(content->rect().contains(QRect(widget->mapTo(content.get(), QPoint()), widget->size())));
    }
    EXPECT_LE(page->m_error->height(), 64);
    // Optional artifact for inspecting this layout; contains synthetic data.
    const auto preview = qgetenv("DSS_LOCK_PASSWORD_PREVIEW");
    if (!preview.isEmpty()) {
        EXPECT_TRUE(content->grab().save(QString::fromLocal8Bit(preview)));
    }
}

TEST_F(UT_LockPasswordGate, EagleFormKeepsHeaderOnLargeScreensAndPreservesInputsAcrossResize)
{
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    content->resize(1920, 1080);
    content->show();
    auto *page = content->m_passwordChangeWidget;
    emit model->lockPasswordChangeForm(true, false, false, {});
    QTest::qWait(30);
    EXPECT_TRUE(content->m_centerTopFrame->isVisible());
    EXPECT_GT(content->m_centerSpacerItem->sizeHint().height(), 0);
    EXPECT_GT(page->mapTo(content.get(), QPoint()).y(), content->m_centerTopFrame->height());
    EXPECT_FALSE(page->m_blurEffectWidget->isHidden());
    EXPECT_FALSE(page->m_strength->isHidden());
    EXPECT_EQ(page->m_submit->text(), QStringLiteral("Save"));
    page->m_current->setText(QStringLiteral("old"));
    page->m_password->setText(QStringLiteral("typed-new"));
    page->m_confirmation->setText(QStringLiteral("typed-new"));
    page->m_hint->setText(QStringLiteral("typed hint"));
    content->resize(1024, 768);
    emit model->lockPasswordChangeForm(true, false, false, QStringLiteral("Policy error\n").repeated(80));
    QTest::qWait(30);
    EXPECT_FALSE(content->m_centerTopFrame->isVisible());
    for (auto *widget : QList<QWidget *> {page->m_current, page->m_password, page->m_confirmation, page->m_hint, page->m_submit}) {
        EXPECT_TRUE(content->rect().contains(QRect(widget->mapTo(content.get(), QPoint()), widget->size())));
    }
    content->resize(1920, 1080);
    emit model->lockPasswordChangeForm(true, false, false, {});
    QTest::qWait(30);
    EXPECT_TRUE(content->m_centerTopFrame->isVisible());
    EXPECT_EQ(page->m_password->text(), QStringLiteral("typed-new"));
    EXPECT_EQ(page->m_confirmation->text(), QStringLiteral("typed-new"));
    EXPECT_EQ(page->m_hint->text(), QStringLiteral("typed hint"));
    const auto preview = qgetenv("DSS_EAGLE_UI_PREVIEW");
    if (!preview.isEmpty()) {
        EXPECT_TRUE(content->grab().save(QString::fromLocal8Bit(preview)));
    }
}

TEST_F(UT_LockPasswordGate, EagleFrontCheckFollowsHostPolicyAndCanBeDisabledBeforeSubmission)
{
    LockPasswordChangeWidget page;
    page.setUser(model->currentUser());
    page.setState(false, false, false, {});
    page.setFixedWidth(360);
    page.show();
    QSignalSpy requests(&page, &LockPasswordChangeWidget::requestChangePassword);
    EXPECT_TRUE(page.m_localSecurityCheck);
    page.m_password->setText(QStringLiteral("1"));
    page.m_confirmation->setText(QStringLiteral("1"));
    QTest::mouseClick(page.m_submit, Qt::LeftButton);
    // The actual library follows /etc/deepin/dde.conf; a host with
    // STRONG_PASSWORD=false deliberately accepts "1", just as eagle does.
    const auto policy = deepin_pw_check("test-domain-user", "1", LEVEL_STRICT_CHECK, nullptr);
    const bool accepted = policy == PW_NO_ERR || policy == PW_ERR_PW_REPEAT;
    EXPECT_EQ(requests.count(), accepted ? 1 : 0);
    EXPECT_EQ(page.m_password->isAlert(), !accepted);
    EXPECT_EQ(page.m_password->text(), QStringLiteral("1"));
    // Exercise the state changed by the dconfig binding without touching the
    // host's configuration. Requests here have no passwd/worker connection.
    requests.clear();
    page.m_localSecurityCheck = false;
    QTest::mouseClick(page.m_submit, Qt::LeftButton);
    ASSERT_EQ(requests.count(), 1);
    EXPECT_EQ(requests.last()[1].toString(), QStringLiteral("1"));
    requests.clear();
    page.m_localSecurityCheck = true;
    page.m_password->setText(QStringLiteral("v%K6x!A9q#T2"));
    page.m_confirmation->setText(QStringLiteral("v%K6x!A9q#T2"));
    page.m_hint->setText(QStringLiteral("optional"));
    QTest::mouseClick(page.m_submit, Qt::LeftButton);
    ASSERT_EQ(requests.count(), 1);
    EXPECT_EQ(requests.last()[3].toString(), QStringLiteral("optional"));
    EXPECT_EQ(worker->m_passwordChange->m_process, nullptr);
}

TEST_F(UT_LockPasswordGate, EaglePasswordControlsProtectInputAndClearVisiblePasswordOnBusy)
{
    LockPasswordChangeWidget page;
    page.setState(true, false, false, {});
    page.setFixedWidth(360);
    page.show();
    auto *edit = page.m_password;
    edit->lineEdit()->setFocus();
    QTest::keyClicks(edit->lineEdit(), "synthetic");
    const auto before = edit->text();
    QApplication::clipboard()->setText(QStringLiteral("pasted"));
    QTest::keyClick(edit->lineEdit(), Qt::Key_V, Qt::ControlModifier);
    QTest::keyClick(edit->lineEdit(), Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(edit->text(), before);
    QTest::mousePress(edit->passwordShowButton(), Qt::LeftButton);
    EXPECT_EQ(edit->echoMode(), QLineEdit::Normal);
    page.setState(true, false, true, {});
    EXPECT_TRUE(edit->text().isEmpty());
    EXPECT_FALSE(edit->m_passwordVisible);
    EXPECT_EQ(edit->echoMode(), QLineEdit::Password);
    EXPECT_FALSE(page.m_cancel->isEnabled());
    EXPECT_FALSE(page.m_submit->isEnabled());
    page.setState(true, false, false, {});
    QTest::keyClick(edit->lineEdit(), Qt::Key_Z, Qt::ControlModifier);
    EXPECT_TRUE(edit->text().isEmpty());
    EXPECT_EQ(page.m_hint->lineEdit()->maxLength(), 14);
    EXPECT_EQ(page.m_hint->lineEdit()->alignment(), Qt::AlignCenter);
}

TEST_F(UT_LockPasswordGate, ClearingPasswordFormDiscardsDraftHintUndoAndRedo)
{
    LockPasswordChangeWidget page;
    page.setFixedWidth(360);
    page.show();
    auto *hint = page.m_hint->lineEdit();
    hint->setFocus();
    QTest::keyClicks(hint, "draft-secret");
    ASSERT_EQ(hint->text(), QStringLiteral("draft-secret"));
    page.clearInputs();
    ASSERT_TRUE(hint->text().isEmpty());
    EXPECT_FALSE(hint->isUndoAvailable());
    EXPECT_FALSE(hint->isRedoAvailable());
    QTest::keyClick(hint, Qt::Key_Z, Qt::ControlModifier);
    EXPECT_TRUE(hint->text().isEmpty());
    QTest::keyClick(hint, Qt::Key_Y, Qt::ControlModifier);
    EXPECT_TRUE(hint->text().isEmpty());
    // Reusing and clearing an already empty form must not recover older input.
    QTest::keyClicks(hint, "next-draft");
    page.clearInputs();
    page.clearInputs();
    QTest::keyClick(hint, Qt::Key_Z, Qt::ControlModifier);
    EXPECT_TRUE(hint->text().isEmpty());
}

TEST_F(UT_LockPasswordGate, FormActivityRenewsIdleAndTimeoutRestoresAuthentication)
{
    AuthenticationLookupTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(DSS_DBUS::accountsPath, &service));
    QTest::qWait(20);
    auto content = managedContent();
    auto *authenticationPage = content->m_stackedLayout->currentWidget();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("old");
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    auto *page = content->m_passwordChangeWidget;
    content->show();
    page->m_password->setText(QStringLiteral("new"));
    page->m_confirmation->setText(QStringLiteral("new"));
    page->m_hint->setText(QStringLiteral("hint"));
    worker->m_passwordChange->m_timer.start(50);
    QTest::keyClick(page->m_hint->lineEdit(), Qt::Key_A);
    EXPECT_GT(worker->m_passwordChange->m_timer.remainingTime(), 50000);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    service.lookups = 0;
    worker->m_passwordChange->m_timer.start(1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service.lookups == 1; }, 3000));
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(model->lockPasswordChangePageVisible());
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_EQ(model->currentModeState(), SessionBaseModel::PasswordMode);
    EXPECT_EQ(content->m_stackedLayout->currentWidget(), authenticationPage);
    EXPECT_EQ(content->m_centerWidget, content->m_authWidget->getAuthWidget());
    EXPECT_TRUE(page->isHidden());
    EXPECT_TRUE(page->m_current->text().isEmpty());
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_TRUE(page->m_confirmation->text().isEmpty());
    EXPECT_TRUE(page->m_hint->text().isEmpty());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(model->visible());
    EXPECT_FALSE(worker->m_passwordChangePassed);
    EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
    EXPECT_TRUE(worker->m_passwordChange->m_newPassword.isEmpty());
    EXPECT_TRUE(worker->m_passwordChangeToken.isEmpty());
    EXPECT_TRUE(worker->m_passwordChangeHint.isEmpty());

    // A new successful authentication must still enter the expired-password
    // flow, without reusing the password from the canceled attempt.
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return model->lockPasswordChangePageVisible() && page->m_currentRequired && !page->m_busy;
    }, 3000));
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(page->m_current->text().isEmpty());
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_TRUE(page->m_confirmation->text().isEmpty());
    EXPECT_TRUE(page->m_hint->text().isEmpty());
}

TEST_F(UT_LockPasswordGate, SuspendDiscardsNewAndConfirmationAndKeepsCurrentPageAccessible)
{
    for (auto stage : {LockPasswordChange::NewPassword, LockPasswordChange::ConfirmPassword}) {
        auto content = managedContent();
        model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
        worker->m_passwordChangeToken = QStringLiteral("old");
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
        }, 3000));
        auto *page = content->m_passwordChangeWidget;
        if (stage == LockPasswordChange::ConfirmPassword) {
            worker->m_passwordChange->submit(QStringLiteral("new"));
        }
        page->m_password->setText(QStringLiteral("new"));
        page->m_confirmation->setText(QStringLiteral("new"));
        page->m_hint->setText(QStringLiteral("hint"));
        DBusLockAgent agent;
        agent.setModel(model);
        agent.Suspend(true);
        agent.Suspend(false);
        agent.Show();
        EXPECT_TRUE(page->m_checkingOnly);
        EXPECT_TRUE(page->m_currentRequired);
        EXPECT_TRUE(page->m_password->text().isEmpty());
        EXPECT_TRUE(page->m_confirmation->text().isEmpty());
        EXPECT_TRUE(page->m_hint->text().isEmpty());
        EXPECT_TRUE(worker->m_passwordChange->m_oldPassword.isEmpty());
        EXPECT_TRUE(worker->m_passwordChange->m_newPassword.isEmpty());
        EXPECT_TRUE(model->visible());
        EXPECT_FALSE(model->isBlackMode());
        EXPECT_EQ(content->m_centerWidget, page);
        worker->cancelPasswordChange();
        QTest::qWait(30);
    }
}

class PasswordHintTestService : public QDBusVirtualObject
{
public:
    QString hint;
    int calls = 0;
    bool fail = false;
    ~PasswordHintTestService() override
    {
        QDBusConnection::systemBus().unregisterObject(path);
        QDBusConnection::systemBus().unregisterService(DSS_DBUS::accountsService);
    }
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.interface() != DSS_DBUS::accountsUserInterface || message.member() != QStringLiteral("SetPasswordHint")) {
            return false;
        }
        ++calls;
        hint = message.arguments().value(0).toString();
        bus.send(fail ? message.createErrorReply(QDBusError::AccessDenied, QStringLiteral("denied")) : message.createReply());
        return true;
    }
    const QString path = QStringLiteral("/org/deepin/dde/Accounts1/UserHintTest");
};

TEST_F(UT_LockPasswordGate, PasswordHintUsesAccounts1AndFailureDoesNotRepeatPasswordChange)
{
    PasswordHintTestService service;
    auto bus = QDBusConnection::systemBus();
    ASSERT_TRUE(bus.registerService(DSS_DBUS::accountsService));
    ASSERT_TRUE(bus.registerVirtualObject(service.path, &service));
    worker->savePasswordHint(service.path, QStringLiteral("a harmless hint"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return service.calls == 1; }, 3000));
    EXPECT_EQ(service.hint, QStringLiteral("a harmless hint"));
    // An empty optional hint clears the previous hint. Metadata failure must
    // neither submit passwd again nor revoke an already changed password.
    service.fail = true;
    worker->savePasswordHint(service.path, QString());
    ASSERT_TRUE(QTest::qWaitFor([&] { return service.calls == 2; }, 3000));
    QTest::qWait(20);
    EXPECT_TRUE(service.hint.isEmpty());
    EXPECT_FALSE(worker->m_passwordChange->isActive());
    EXPECT_EQ(worker->m_passwordChange->m_process, nullptr);
}

TEST_F(UT_LockPasswordGate, GreeterSharesFormWithoutEnablingLockGate)
{
    model->setAppType(AuthCommon::Login);
    auto content = managedContent();
    model->setGreeterPasswordChangeActive(true);
    emit model->greeterPasswordChangePageChanged();
    emit model->lockPasswordChangeForm(true, false, false, {});
    ASSERT_NE(content->m_passwordChangeWidget, nullptr);
    auto *page = content->m_passwordChangeWidget;
    EXPECT_EQ(content->m_centerWidget, page);
    EXPECT_EQ(content->m_stackedLayout->currentIndex(), 0);
    EXPECT_FALSE(model->lockPasswordChangeActive());
    EXPECT_FALSE(page->m_current->isHidden());
    EXPECT_FALSE(page->m_password->isHidden());
    EXPECT_FALSE(page->m_confirmation->isHidden());
    EXPECT_FALSE(page->m_hint->isHidden());
    EXPECT_EQ(page->m_submit->text(), QStringLiteral("Save"));
    EXPECT_EQ(page->m_cancel->text(), QStringLiteral("Cancel"));
    QSignalSpy canceled(content.get(), &LockContent::requestCancelPasswordChange);
    QTest::keyClick(content.get(), Qt::Key_Escape);
    EXPECT_EQ(canceled.count(), 1);
    page->m_password->setText(QStringLiteral("temporary"));
    model->setGreeterPasswordChangeActive(false);
    emit model->greeterPasswordChangePageChanged();
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_EQ(content->m_centerWidget, content->m_authWidget->getAuthWidget());
}

TEST_F(UT_LockPasswordChange, TruncatedProcessFailureNeverDisplaysPasswordFragments)
{
    for (const auto &secret : {QByteArray("$old"), QByteArray("$new")}) {
        const auto executable = program("IFS= read -r old\nIFS= read -r new\nIFS= read -r confirmation\n"
            "printf '%8188s%s' '' \"" + secret + "\"\nexit 1\n");
        LockPasswordChange change(nullptr, {}, executable);
        QSignalSpy prompts(&change, &LockPasswordChange::promptChanged);
        change.begin("test-domain-user", "OldSecret!", true, false);
        ASSERT_TRUE(wait([&] { return change.m_stage == LockPasswordChange::NewPassword; }));
        change.submit("NewSecret!");
        change.submit("NewSecret!");
        ASSERT_TRUE(wait([&] { return !prompts.last()[1].toString().isEmpty(); }));
        EXPECT_EQ(prompts.last()[1].toString(), "Password change failed. Please try again.");
    }
}

TEST_F(UT_LockPasswordChange, ChunkedOutputRedactionHandlesExactLimitAndResetsForRetry)
{
    LockPasswordChange change;
    change.m_oldPassword = QStringLiteral("OldSecret!");
    change.m_newPassword = QStringLiteral("NewSecret!");
    change.appendOutput(QByteArray(8182, ' '));
    change.appendOutput("OldSec");
    change.appendOutput("ret!");
    EXPECT_FALSE(change.m_outputTruncated);
    EXPECT_EQ(change.sanitizedOutput(), "[redacted]");
    change.appendOutput("extra");
    EXPECT_TRUE(change.m_outputTruncated);
    EXPECT_EQ(change.sanitizedOutput(), "Password change failed. Please try again.");
    change.clearPasswords();
    change.appendOutput("policy rejected");
    EXPECT_FALSE(change.m_outputTruncated);
    EXPECT_EQ(change.sanitizedOutput(), "policy rejected");
}

TEST_F(UT_LockPasswordGate, ScreenTransferPreservesAllFormInputsButRealHideClearsThem)
{
    QWidget firstScreen;
    QWidget secondScreen;
    firstScreen.show();
    secondScreen.show();
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    content->setParent(&firstScreen);
    content->show();
    auto *page = content->m_passwordChangeWidget;
    page->m_current->setText("old");
    page->m_password->setText("new");
    page->m_confirmation->setText("new");
    page->m_hint->setText("hint");
    // FullScreenBackground::setContent transfers this same shared content.
    content->setParent(nullptr);
    content->hide();
    content->setParent(&secondScreen);
    content->show();
    QCoreApplication::processEvents();
    EXPECT_EQ(page->m_current->text(), "old");
    EXPECT_EQ(page->m_password->text(), "new");
    EXPECT_EQ(page->m_confirmation->text(), "new");
    EXPECT_EQ(page->m_hint->text(), "hint");
    EXPECT_TRUE(model->lockPasswordChangeActive());
    content->hide();
    QCoreApplication::processEvents();
    EXPECT_TRUE(page->m_current->text().isEmpty());
    EXPECT_TRUE(page->m_password->text().isEmpty());
    EXPECT_TRUE(page->m_confirmation->text().isEmpty());
    EXPECT_TRUE(page->m_hint->text().isEmpty());
}

TEST_F(UT_LockPasswordGate, SessionHideClearsFormEvenWhenImmediatelyShownAgain)
{
    auto content = managedContent();
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return content->m_passwordChangeWidget && !content->m_passwordChangeWidget->m_busy;
    }, 3000));
    content->show();
    auto *page = content->m_passwordChangeWidget;
    page->m_password->setText("new");
    emit model->visibleChanged(false);
    emit model->visibleChanged(true);
    EXPECT_TRUE(page->m_password->text().isEmpty());
}

TEST_F(UT_LockPasswordGate, MfaCancelRestoresLiveWidgetAcrossRepeatedChanges)
{
    auto content = managedContent();
    const auto factors = AUTH_FLAGS_CAST(AT_Password | AT_Fingerprint);
    model->setAuthType(factors);
    model->updateMFAFlag(true);
    ASSERT_NE(content->m_mfaWidget, nullptr);
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    QSignalSpy sequence(&MFASequenceControl::instance(), &MFASequenceControl::currentUIAuthTypeChanged);
    QObject::connect(&MFASequenceControl::instance(), &MFASequenceControl::currentUIAuthTypeChanged,
                     content.get(), [&](int) {
        if (model->getAuthProperty().MFAFlag) {
            EXPECT_NE(content->m_mfaWidget, nullptr);
            EXPECT_EQ(content->m_authWidget, content->m_mfaWidget);
        }
    });
    for (int i = 0; i < 3; ++i) {
        QPointer<MFAWidget> previousMfa = content->m_mfaWidget;
        worker->m_passwordChangeToken = QStringLiteral("old");
        worker->authFinishedAction();
        ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
        EXPECT_TRUE(previousMfa.isNull());
        ASSERT_NE(content->m_sfaWidget, nullptr);
        QPointer<SFAWidget> temporarySfa = content->m_sfaWidget;
        const int before = sequence.count();
        // Cancel through the actual shared page's signal wiring.
        emit content->m_passwordChangeWidget->requestCancel();
        EXPECT_TRUE(temporarySfa.isNull());
        EXPECT_GT(sequence.count(), before);
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(model->getAuthProperty().AuthType, factors);
        EXPECT_EQ(content->m_authWidget, content->m_mfaWidget);
        EXPECT_EQ(content->m_centerWidget.data(), content->m_mfaWidget->getAuthWidget());
        EXPECT_FALSE(model->lockPasswordChangeActive());
        EXPECT_EQ(finished.count(), 0);
        QCoreApplication::processEvents();
    }
}

TEST_F(UT_LockPasswordGate, MfaCompletionRestoresLiveWidgetBeforeUnlock)
{
    auto content = managedContent();
    const auto factors = AUTH_FLAGS_CAST(AT_Password | AT_Fingerprint);
    model->setAuthType(factors);
    model->updateMFAFlag(true);
    ASSERT_NE(content->m_mfaWidget, nullptr);
    model->currentUser()->updatePasswordExpiredState(User::ExpiredAlready, 0);
    worker->m_passwordChangeToken = QStringLiteral("old");
    QSignalSpy finished(model, &SessionBaseModel::authFinished);
    worker->authFinishedAction();
    ASSERT_TRUE(QTest::qWaitFor([&] { return model->lockPasswordChangePageVisible(); }, 3000));
    QPointer<SFAWidget> temporarySfa = content->m_sfaWidget;
    ASSERT_FALSE(temporarySfa.isNull());
    QObject::connect(model, &SessionBaseModel::authFinished, content.get(), [&](bool success) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(temporarySfa.isNull());
        EXPECT_TRUE(model->getAuthProperty().MFAFlag);
        EXPECT_EQ(content->m_authWidget, content->m_mfaWidget);
        EXPECT_FALSE(model->lockPasswordChangeActive());
    });
    // Process I/O is covered separately; exercise its real completion callbacks.
    worker->m_passwordChange->finish(true);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_TRUE(temporarySfa.isNull());
    EXPECT_EQ(model->getAuthProperty().AuthType, factors);
    EXPECT_EQ(model->currentUser()->expiredState(), User::ExpiredNormal);
    QCoreApplication::processEvents();
    EXPECT_EQ(finished.count(), 1);
}
