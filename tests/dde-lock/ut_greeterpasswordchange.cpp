// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeterpasswordchange.h"
#include "../gettext_test_catalog.h"
#include <QDataStream>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <clocale>
#include <gtest/gtest.h>
#include <libintl.h>

TEST(UT_GreeterPasswordChange, RespondsOnlyToSecretPromptsAfterSave)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start();
    change.prompt(QStringLiteral("Current password:"), true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
    change.submit("old", "newSecret!", "newSecret!", "hint");
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 1);
    EXPECT_EQ(responses[0][0].toString(), "old");
    change.message(false);
    EXPECT_EQ(responses.count(), 1);
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 3);
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    EXPECT_TRUE(change.finish(true));
    EXPECT_EQ(change.takeHint(), "hint");
    EXPECT_TRUE(change.takeHint().isEmpty());
    EXPECT_FALSE(change.active());
}

TEST(UT_GreeterPasswordChange, ReusesVerifiedPasswordAndAllowsPamToSkipOldPrompt)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    EXPECT_FALSE(change.currentRequired());
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 2);
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, BatchedPromptsWaitForSaveAndPreserveEveryPasswordRole)
{
    for (bool passwordless : {false, true}) {
        for (bool currentPrompt : {false, true}) {
            GreeterPasswordChange change;
            QSignalSpy responses(&change, &GreeterPasswordChange::respond);
            change.start(passwordless ? QString() : QStringLiteral("old"), {}, passwordless);
            if (currentPrompt)
                change.prompt("(current) NT password:", true);
            change.prompt("Enter new NT password:", true);
            change.prompt("Retype new NT password:", true);
            QCoreApplication::processEvents();
            EXPECT_EQ(responses.count(), 0);
            ASSERT_TRUE(change.active());
            change.submit({}, "newSecret!", "newSecret!", "hint");
            EXPECT_EQ(responses.count(), 0);
            ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == (currentPrompt ? 3 : 2); }, 3000));
            if (currentPrompt)
                EXPECT_EQ(responses[0][0].toString(), passwordless ? QString() : QStringLiteral("old"));
            EXPECT_EQ(responses[responses.count() - 2][0].toString(), "newSecret!");
            EXPECT_EQ(responses.last()[0].toString(), "newSecret!");
            EXPECT_TRUE(change.finish(true));
            EXPECT_EQ(change.takeHint(), "hint");
        }
    }
}

TEST(UT_GreeterPasswordChange, ExpiryAfterCurrentResponsePreservesSubmittedReplacement)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    QSignalSpy forms(&change, &GreeterPasswordChange::formChanged);
    change.start();
    change.prompt("Password:", true);
    change.submit("old", "newSecret!", "newSecret!", "hint");
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    EXPECT_EQ(responses[0][0].toString(), "old");
    forms.clear();
    change.message(true);
    change.message(true);
    change.prompt("New password:", true);
    change.prompt("Retype new password:", true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_TRUE(change.busy());
    for (const auto &form : forms)
        EXPECT_TRUE(form[2].toString().isEmpty());
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    EXPECT_TRUE(change.finish(true));
    EXPECT_EQ(change.takeHint(), "hint");
}

TEST(UT_GreeterPasswordChange, RejectedBatchedPromptsWaitForCorrectedSave)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    change.prompt("Enter new NT password:", true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    change.message(true);
    change.prompt("Enter new NT password:", true);
    change.prompt("Retype new NT password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 1);
    EXPECT_FALSE(change.busy());
    change.submit({}, "BetterNew!", "BetterNew!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 3; }, 3000));
    EXPECT_EQ(responses[1][0].toString(), "BetterNew!");
    EXPECT_EQ(responses[2][0].toString(), "BetterNew!");
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, UnknownQuestionInvalidatesWholeUndispatchedBatch)
{
    for (bool secret : {false, true}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("old");
        change.submit({}, "newSecret!", "newSecret!", {});
        change.prompt("New password:", true);
        change.prompt("Verification code:", secret);
        QCoreApplication::processEvents();
        EXPECT_EQ(responses.count(), 0);
        EXPECT_FALSE(change.active());
        EXPECT_TRUE(change.m_current.isEmpty());
        EXPECT_TRUE(change.m_password.isEmpty());
    }
}

TEST(UT_GreeterPasswordChange, ResetAndCancelInvalidateQueuedResponses)
{
    for (bool cancel : {false, true}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("old");
        change.submit({}, "newSecret!", "newSecret!", {});
        change.prompt("New password:", true);
        change.prompt("Retype new password:", true);
        cancel ? change.cancel() : change.reset();
        change.start("fresh-old");
        QCoreApplication::processEvents();
        EXPECT_EQ(responses.count(), 0);
        EXPECT_TRUE(change.active());
        EXPECT_FALSE(change.busy());
    }
}

TEST(UT_GreeterPasswordChange, SynchronousCancellationStopsRemainingBatchResponses)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    QObject::connect(&change, &GreeterPasswordChange::respond, &change, [&] { change.cancel(); });
    change.start("old");
    change.prompt("New password:", true);
    change.prompt("Retype new password:", true);
    change.submit({}, "newSecret!", "newSecret!", {});
    ASSERT_TRUE(QTest::qWaitFor([&] { return responses.count() == 1; }, 3000));
    QCoreApplication::processEvents();
    EXPECT_EQ(responses[0][0].toString(), "newSecret!");
    EXPECT_EQ(responses.count(), 1);
    EXPECT_FALSE(change.active());
    EXPECT_TRUE(change.m_password.isEmpty());
}

TEST(UT_GreeterPasswordChange, ErrorsBeforeSaveAreNotPasswordChangeFailures)
{
    GreeterPasswordChange change;
    QSignalSpy forms(&change, &GreeterPasswordChange::formChanged);
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    const auto initialForms = forms.count();
    // PAM_ERROR_MSG can announce mandatory expiry before requesting a new
    // password. No replacement has been submitted at this point.
    change.message(true);
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(forms.count(), initialForms);
    EXPECT_FALSE(change.m_errorReceived);
    EXPECT_FALSE(change.busy());
    EXPECT_EQ(responses.count(), 0);

    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 1);
    change.message(true);
    EXPECT_TRUE(change.m_errorReceived);
    EXPECT_FALSE(forms.last()[2].toString().isEmpty());
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    EXPECT_FALSE(change.busy());
    EXPECT_EQ(responses.count(), 1);
    change.submit({}, "BetterNew!", "BetterNew!", {});
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, FreshLoginRetryCanRequestOldPasswordTwice)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start();
    change.submit("old", "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("Password:", true);
    QCoreApplication::processEvents();
    change.prompt("(current) UNIX password:", true);
    QCoreApplication::processEvents();
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 4);
    EXPECT_EQ(responses[0][0].toString(), "old");
    EXPECT_EQ(responses[1][0].toString(), "old");
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, PasswordlessConversationsAcceptEmptyCurrentAndIgnoreStaleCredentials)
{
    for (const bool requestCurrent : {false, true}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("stale-cache", {}, true);
        EXPECT_FALSE(change.currentRequired());
        change.prompt(requestCurrent ? "Current password:" : "New password:", true);
        QCoreApplication::processEvents();
        change.submit("invented-old", "newSecret!", "newSecret!", "stale-cache");
        QCoreApplication::processEvents();
        ASSERT_TRUE(change.busy());
        if (requestCurrent) {
            ASSERT_EQ(responses.count(), 1);
            EXPECT_TRUE(responses[0][0].toString().isEmpty());
            change.prompt("New password:", true);
            QCoreApplication::processEvents();
        }
        change.prompt("Retype new password:", true);
        QCoreApplication::processEvents();
        ASSERT_EQ(responses.count(), requestCurrent ? 3 : 2);
        EXPECT_EQ(responses.last()[0].toString(), "newSecret!");
        EXPECT_TRUE(change.finish(true));
        EXPECT_EQ(change.takeHint(), "stale-cache");
    }
}

TEST(UT_GreeterPasswordChange, PasswordlessRejectionAllowsCorrectionAndResetRestoresCredentialRequirement)
{
    for (const auto retryPrompt : {"Current password:", "New password:"}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start({}, {}, true);
        change.prompt("New password:", true);
        QCoreApplication::processEvents();
        change.submit({}, "newSecret!", "newSecret!", "hint");
        QCoreApplication::processEvents();
        ASSERT_EQ(responses.count(), 1);
        change.message(true);
        change.prompt(retryPrompt, true);
        QCoreApplication::processEvents();
        EXPECT_FALSE(change.currentRequired());
        EXPECT_FALSE(change.busy());
        EXPECT_EQ(responses.count(), 1);
        change.submit({}, "BetterNew!", "BetterNew!", {});
        QCoreApplication::processEvents();
        if (QString::fromLatin1(retryPrompt).startsWith("Current")) {
            EXPECT_TRUE(responses.last()[0].toString().isEmpty());
            change.prompt("New password:", true);
            QCoreApplication::processEvents();
        }
        change.prompt("Retype new password:", true);
        QCoreApplication::processEvents();
        EXPECT_TRUE(change.finish(true));
        change.start();
        EXPECT_TRUE(change.currentRequired());
        change.submit({}, "AnotherNew!", "AnotherNew!", {});
        QCoreApplication::processEvents();
        EXPECT_FALSE(change.busy());
    }
}

TEST(UT_GreeterPasswordChange, PasswordlessAccountsStillRejectInvalidNewPasswordsAndUnsafeHints)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    QSignalSpy forms(&change, &GreeterPasswordChange::formChanged);
    change.start({}, {}, true);
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    for (const auto &password : {QString(), QString::fromUtf8("新密码"), QString("new\nsecret")}) {
        change.submit({}, password, password, {});
        QCoreApplication::processEvents();
        EXPECT_FALSE(change.busy());
        EXPECT_FALSE(forms.last()[2].toString().isEmpty());
    }
    change.submit({}, "newSecret!", "different", {});
    QCoreApplication::processEvents();
    change.submit({}, "newSecret!", "newSecret!", "newSecret!");
    QCoreApplication::processEvents();
    change.submit({}, "newSecret!", "newSecret!", QString(15, QLatin1Char('x')));
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 1);
    EXPECT_EQ(responses[0][0].toString(), "newSecret!");
}

TEST(UT_GreeterPasswordChange, RejectsUnsupportedAndMismatchedPasswordsBeforeResponding)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    QSignalSpy forms(&change, &GreeterPasswordChange::formChanged);
    change.start();
    change.prompt("Current password:", true);
    QCoreApplication::processEvents();
    for (const QString &password : {QString(), QString("old"), QString::fromUtf8("新密码🙂"), QString("new\nsecret")}) {
        change.submit("old", password, password, {});
        QCoreApplication::processEvents();
        EXPECT_FALSE(change.busy());
        EXPECT_FALSE(forms.last()[2].toString().isEmpty());
    }
    change.submit("old", "newSecret!", "different", {});
    QCoreApplication::processEvents();
    change.submit("old", "newSecret!", "newSecret!", "newSecret!");
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
}

TEST(UT_GreeterPasswordChange, BusySubmissionIsIgnoredAndBackendTextCannotRevealSecrets)
{
    GreeterPasswordChange change;
    QSignalSpy forms(&change, &GreeterPasswordChange::formChanged);
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.prompt("Current password:", true);
    QCoreApplication::processEvents();
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.submit({}, "other!", "other!", {});
    QCoreApplication::processEvents();
    change.message(true);
    EXPECT_EQ(responses.count(), 1);
    EXPECT_TRUE(change.busy());
    EXPECT_FALSE(forms.last()[2].toString().contains("old"));
    EXPECT_FALSE(forms.last()[2].toString().contains("newSecret!"));
}

TEST(UT_GreeterPasswordChange, CancelAndSleepResetInvalidateDelayedCompletions)
{
    for (const bool cancel : {true, false}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("old");
        change.submit({}, "newSecret!", "newSecret!", "hint");
        QCoreApplication::processEvents();
        change.prompt("New password:", true);
        QCoreApplication::processEvents();
        const int before = responses.count();
        cancel ? change.cancel() : change.reset();
        change.prompt("Retype new password:", true);
        QCoreApplication::processEvents();
        EXPECT_EQ(responses.count(), before);
        EXPECT_FALSE(change.finish(true));
        EXPECT_TRUE(change.takeHint().isEmpty());
    }
}

TEST(UT_GreeterPasswordChange, OnlyLightdmSuccessAfterConfirmationCountsAsChanged)
{
    for (const bool success : {false, true}) {
        GreeterPasswordChange change;
        change.start("old");
        change.submit({}, "newSecret!", "newSecret!", "hint");
        QCoreApplication::processEvents();
        change.prompt("New password:", true);
        QCoreApplication::processEvents();
        EXPECT_FALSE(change.finish(success));
        EXPECT_TRUE(change.takeHint().isEmpty());
    }
    GreeterPasswordChange change;
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", "hint");
    QCoreApplication::processEvents();
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    EXPECT_FALSE(change.finish(false));
    EXPECT_TRUE(change.takeHint().isEmpty());
}

TEST(UT_GreeterPasswordChange, IdleActivityRenewsOneMinuteTimerAndBusyHasBoundedTimeout)
{
    GreeterPasswordChange change;
    QSignalSpy canceled(&change, &GreeterPasswordChange::canceled);
    change.start();
    EXPECT_EQ(change.m_idleTimer.interval(), 60000);
    change.m_idleTimer.start(10);
    change.m_idleTimer.setInterval(60000);
    change.resetIdleTimer();
    EXPECT_GT(change.m_idleTimer.remainingTime(), 50000);
    change.m_idleTimer.start(1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canceled.count() == 1; }, 500));
    EXPECT_FALSE(change.active());
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    EXPECT_FALSE(change.m_idleTimer.isActive());
    EXPECT_EQ(change.m_operationTimer.interval(), 60000);
    change.m_operationTimer.start(1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canceled.count() == 2; }, 500));
}

TEST(UT_GreeterPasswordChange, RepeatedNewPromptStopsRejectedSecretReplay)
{
    GreeterPasswordChange change;
    QSignalSpy canceled(&change, &GreeterPasswordChange::canceled);
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 1);
    EXPECT_EQ(canceled.count(), 0);
    EXPECT_TRUE(change.active());
    EXPECT_FALSE(change.busy());
    change.submit({}, "BetterNew!", "BetterNew!", {});
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 3);
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, QuestionPromptsNeverReceivePasswords)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("Continue?", false);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
    EXPECT_FALSE(change.active());
}

TEST(UT_GreeterPasswordChange, DetectsPamChauthtokWithoutCachedExpiry)
{
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("New UNIX password:"));
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(QString::fromUtf8("请输入新密码：")));
    EXPECT_TRUE(GreeterPasswordChange::isExpiryMessage("Your password has expired."));
    EXPECT_TRUE(GreeterPasswordChange::isExpiryMessage(QString::fromUtf8("密码已过期")));
    EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Password:"));
    EXPECT_FALSE(GreeterPasswordChange::isExpiryMessage("Password will expire in 3 days"));
}

TEST(UT_GreeterPasswordChange, UnknownSecretPromptCannotReceivePassword)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("Verification code:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 0);
    EXPECT_FALSE(change.active());
}

TEST(UT_GreeterPasswordChange, IncorrectOldPasswordIsNotReplayed)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start();
    change.prompt("Current password:", true);
    QCoreApplication::processEvents();
    change.submit("incorrect", "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.message(true);
    change.prompt("Password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 1);
    EXPECT_FALSE(change.busy());
    EXPECT_TRUE(change.currentRequired());
    change.submit("correct", "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 2);
    EXPECT_EQ(responses[1][0].toString(), "correct");
}

TEST(UT_GreeterPasswordChange, PolicyErrorAndNewPromptAllowCorrection)
{
    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    change.message(true);
    change.prompt("New password:", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 1);
    EXPECT_FALSE(change.busy());
    change.submit({}, "BetterNew!", "BetterNew!", {});
    QCoreApplication::processEvents();
    change.prompt("Retype new password:", true);
    QCoreApplication::processEvents();
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, LinuxPamChinesePromptsSendReplacementNotCachedOldPassword)
{
    for (const bool cached : {false, true}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start(cached ? QStringLiteral("old") : QString());
        EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(QStringLiteral("新的密码： ")));
        change.prompt(QStringLiteral("新的密码： "), true);
        QCoreApplication::processEvents();
        change.submit(cached ? QString() : QStringLiteral("old"), "newSecret!", "newSecret!", {});
        QCoreApplication::processEvents();
        change.prompt(QStringLiteral("重新输入新的密码： "), true);
        QCoreApplication::processEvents();
        ASSERT_EQ(responses.count(), 2);
        EXPECT_EQ(responses[0][0].toString(), "newSecret!");
        EXPECT_EQ(responses[1][0].toString(), "newSecret!");
        EXPECT_TRUE(change.finish(true));
    }
}

TEST(UT_GreeterPasswordChange, UnknownPasswordQuestionNeverReceivesCachedCredentials)
{
    for (const auto &prompt : {"Password for another service:", "New password recovery code:", "OTP password:"}) {
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("old");
        change.submit({}, "newSecret!", "newSecret!", {});
        QCoreApplication::processEvents();
        change.prompt(QString::fromLatin1(prompt), true);
        QCoreApplication::processEvents();
        EXPECT_EQ(responses.count(), 0);
        EXPECT_FALSE(change.active());
    }
}

TEST(UT_GreeterPasswordChange, WinbindPromptsPreserveCurrentAndReplacementPasswordRoles)
{
    const QList<QStringList> conversations {
        {QStringLiteral("(current) NT password: "), QStringLiteral("Enter new NT password: "),
         QStringLiteral("Retype new NT password: ")},
        {QStringLiteral("（当前）NT 密码： "), QStringLiteral("输入新 NT 密码： "),
         QStringLiteral("重键入新 NT 密码： ")}
    };
    for (const auto &prompts : conversations) {
        for (bool requestCurrent : {false, true}) {
            GreeterPasswordChange change;
            QSignalSpy responses(&change, &GreeterPasswordChange::respond);
            change.start(QStringLiteral("cached-old"));
            EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(prompts[1]));
            EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(prompts[2]));
            EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt(prompts[0]));
            change.prompt(prompts[requestCurrent ? 0 : 1], true);
            QCoreApplication::processEvents();
            EXPECT_EQ(responses.count(), 0);
            change.submit({}, QStringLiteral("newSecret!"), QStringLiteral("newSecret!"), {});
            QCoreApplication::processEvents();
            ASSERT_EQ(responses.count(), 1);
            EXPECT_EQ(responses[0][0].toString(), requestCurrent ? "cached-old" : "newSecret!");
            if (requestCurrent)
                change.prompt(prompts[1], true);
                QCoreApplication::processEvents();
            change.prompt(prompts[2], true);
            QCoreApplication::processEvents();
            ASSERT_EQ(responses.count(), requestCurrent ? 3 : 2);
            EXPECT_EQ(responses.last()[0].toString(), "newSecret!");
            EXPECT_TRUE(change.finish(true));
        }
    }
}

TEST(UT_GreeterPasswordChange, SssdPromptsPreserveCurrentAndReplacementPasswordRoles)
{
    const QList<QStringList> conversations {
        {QStringLiteral("Current Password: "), QStringLiteral("New Password: "),
         QStringLiteral("Reenter new Password: ")},
        {QStringLiteral("当前密码： "), QStringLiteral("新密码： "),
         QStringLiteral("重新输入新密码： ")},
        {QStringLiteral(" current\tPASSWORD: \n"), QStringLiteral(" NEW password:\t"),
         QStringLiteral(" reenter NEW PASSWORD： ")}
    };
    for (const auto &prompts : conversations) {
        for (bool cached : {false, true}) {
            for (bool requestCurrent : {false, true}) {
                GreeterPasswordChange change;
                QSignalSpy responses(&change, &GreeterPasswordChange::respond);
                QSignalSpy canceled(&change, &GreeterPasswordChange::canceled);
                change.start(cached ? QStringLiteral("old") : QString());
                EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(prompts[1]));
                EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(prompts[2]));
                EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt(prompts[0]));
                change.prompt(prompts[requestCurrent ? 0 : 1], true);
                QCoreApplication::processEvents();
                EXPECT_EQ(responses.count(), 0);
                change.submit(cached ? QString() : QStringLiteral("old"), "newSecret!", "newSecret!", "hint");
                QCoreApplication::processEvents();
                ASSERT_EQ(responses.count(), 1);
                EXPECT_EQ(responses[0][0].toString(), requestCurrent ? "old" : "newSecret!");
                if (requestCurrent)
                    change.prompt(prompts[1], true);
                    QCoreApplication::processEvents();
                change.prompt(prompts[2], true);
                QCoreApplication::processEvents();
                ASSERT_EQ(responses.count(), requestCurrent ? 3 : 2);
                EXPECT_EQ(responses.last()[0].toString(), "newSecret!");
                EXPECT_EQ(canceled.count(), 0);
                EXPECT_TRUE(change.finish(true));
                EXPECT_EQ(change.takeHint(), "hint");
            }
        }
    }
}

TEST(UT_GreeterPasswordChange, WinbindUsesItsOwnGettextCatalog)
{
    // Samba's German translations are deliberately absent from the literal
    // aliases. Build a private catalog to verify the pam_winbind gettext domain.
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("de/LC_MESSAGES")));
    const QMap<QByteArray, QByteArray> messages {
        {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
        {"(current) NT password: ", "(aktuelles) NT Passwort: "},
        {"Enter new NT password: ", "Neues NT Passwort eingeben: "},
        {"Retype new NT password: ", "Neues NT Passwort wiederholen: "}
    };
    ASSERT_TRUE(writeGettextCatalog(directory.filePath(QStringLiteral("de/LC_MESSAGES/pam_winbind.mo")), messages));

    const QByteArray oldLocale = std::setlocale(LC_MESSAGES, nullptr);
    const QByteArray oldLanguage = qgetenv("LANGUAGE");
    const QByteArray oldDirectory = bindtextdomain("pam_winbind", nullptr);
    const auto restore = qScopeGuard([&] {
        bindtextdomain("pam_winbind", oldDirectory.constData());
        oldLanguage.isNull() ? qunsetenv("LANGUAGE") : qputenv("LANGUAGE", oldLanguage);
        std::setlocale(LC_MESSAGES, oldLocale.constData());
    });
    qputenv("LANGUAGE", "de");
    // glibc intentionally disables gettext catalogs in the C.UTF-8 locale.
    if (!std::setlocale(LC_MESSAGES, "de_DE.UTF-8"))
        GTEST_SKIP() << "Requires the de_DE.UTF-8 locale for the gettext integration check";
    bindtextdomain("pam_winbind", directory.path().toUtf8().constData());
    ASSERT_STREQ(dgettext("pam_winbind", "Enter new NT password: "), "Neues NT Passwort eingeben: ");

    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start();
    change.prompt(QStringLiteral("(aktuelles) NT Passwort: "), true);
    QCoreApplication::processEvents();
    change.submit("old", "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("Neues NT Passwort eingeben: "));
    change.prompt(QStringLiteral("Neues NT Passwort eingeben: "), true);
    QCoreApplication::processEvents();
    change.prompt(QStringLiteral("Neues NT Passwort wiederholen: "), true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 3);
    EXPECT_EQ(responses[0][0].toString(), "old");
    EXPECT_EQ(responses[1][0].toString(), "newSecret!");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    EXPECT_TRUE(change.finish(true));
}

TEST(UT_GreeterPasswordChange, SssdUsesItsOwnGettextCatalog)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("de/LC_MESSAGES")));
    // These are SSSD's native msgids and German translations, rather than
    // Linux-PAM's independently translated password conversation.
    const QMap<QByteArray, QByteArray> messages {
        {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
        {"Password: ", "Passwort: "},
        {"Current Password: ", "Aktuelles Passwort: "},
        {"New Password: ", "Neues Passwort: "},
        {"Reenter new Password: ", "Neues Passwort wiederholen: "}
    };
    ASSERT_TRUE(writeGettextCatalog(directory.filePath(QStringLiteral("de/LC_MESSAGES/sssd.mo")), messages));

    const QByteArray oldLocale = std::setlocale(LC_MESSAGES, nullptr);
    const QByteArray oldLanguage = qgetenv("LANGUAGE");
    const QByteArray oldDirectory = bindtextdomain("sssd", nullptr);
    const auto restore = qScopeGuard([&] {
        bindtextdomain("sssd", oldDirectory.constData());
        oldLanguage.isNull() ? qunsetenv("LANGUAGE") : qputenv("LANGUAGE", oldLanguage);
        std::setlocale(LC_MESSAGES, oldLocale.constData());
    });
    qputenv("LANGUAGE", "de");
    if (!std::setlocale(LC_MESSAGES, "de_DE.UTF-8"))
        GTEST_SKIP() << "Requires the de_DE.UTF-8 locale for the gettext integration check";
    bindtextdomain("sssd", directory.path().toUtf8().constData());
    ASSERT_STREQ(dgettext("sssd", "Reenter new Password: "), "Neues Passwort wiederholen: ");

    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    QSignalSpy canceled(&change, &GreeterPasswordChange::canceled);
    change.start();
    change.prompt(QStringLiteral("Passwort: "), true);
    QCoreApplication::processEvents();
    change.submit("old", "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt(QStringLiteral("Aktuelles Passwort: "), true);
    QCoreApplication::processEvents();
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("Neues Passwort: "));
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("Neues Passwort wiederholen: "));
    change.prompt(QStringLiteral("Neues Passwort: "), true);
    QCoreApplication::processEvents();
    change.prompt(QStringLiteral("Neues Passwort wiederholen: "), true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 4);
    EXPECT_EQ(responses[0][0].toString(), "old");
    EXPECT_EQ(responses[1][0].toString(), "old");
    EXPECT_EQ(responses[2][0].toString(), "newSecret!");
    EXPECT_EQ(responses[3][0].toString(), "newSecret!");
    EXPECT_EQ(canceled.count(), 0);
    EXPECT_TRUE(change.finish(true));

    change.start("old");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt(QStringLiteral("Neues Passwort für OTP: "), true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 4);
    EXPECT_EQ(canceled.count(), 1);
    EXPECT_FALSE(change.active());
}

namespace {
class AuthenticationLocaleCatalogs
{
public:
    AuthenticationLocaleCatalogs()
    {
        for (const auto domain : {"Linux-PAM", "sssd", "pam_winbind"})
            oldDirectories.insert(domain, bindtextdomain(domain, nullptr));
    }
    ~AuthenticationLocaleCatalogs()
    {
        for (auto it = oldDirectories.cbegin(); it != oldDirectories.cend(); ++it)
            bindtextdomain(it.key().constData(), it.value().constData());
        oldLanguage.isNull() ? qunsetenv("LANGUAGE") : qputenv("LANGUAGE", oldLanguage);
        std::setlocale(LC_MESSAGES, oldLocale.constData());
    }
    bool initialize()
    {
        if (!directory.isValid())
            return false;
        for (const auto domain : {"Linux-PAM", "sssd", "pam_winbind"})
            bindtextdomain(domain, directory.path().toUtf8().constData());
        qputenv("LANGUAGE", "en:fr");
        return std::setlocale(LC_MESSAGES, "C") != nullptr;
    }
    QString path(const QString &locale, const QString &domain)
    {
        const auto folder = locale + QStringLiteral("/LC_MESSAGES");
        QDir(directory.path()).mkpath(folder);
        return directory.filePath(folder + QLatin1Char('/') + domain + QStringLiteral(".mo"));
    }
    QTemporaryDir directory;
private:
    QMap<QByteArray, QByteArray> oldDirectories;
    QByteArray oldLanguage = qgetenv("LANGUAGE");
    QByteArray oldLocale = std::setlocale(LC_MESSAGES, nullptr);
};
}

TEST(UT_GreeterPasswordChange, AuthenticationLocaleOverridesGreeterLanguageForEveryPamBackend)
{
    AuthenticationLocaleCatalogs catalogs;
    ASSERT_TRUE(catalogs.initialize());
    struct Backend {
        QByteArray domain;
        QStringList messages;
        QStringList translated;
    };
    const QList<Backend> backends {
        {"Linux-PAM", {"Current password: ", "New password: ", "Retype new password: "},
         {"Geben Sie das aktuelle Passwort ein: ", "Geben Sie ein neues Passwort ein: ",
          "Geben Sie das neue Passwort erneut ein: "}},
        {"sssd", {"Current Password: ", "New Password: ", "Reenter new Password: "},
         {"Aktuelles Passwort: ", "Neues Passwort: ", "Neues Passwort wiederholen: "}},
        {"pam_winbind", {"(current) NT password: ", "Enter new NT password: ", "Retype new NT password: "},
         {"(aktuelles) NT Passwort: ", "Neues NT Passwort eingeben: ", "Neues NT Passwort wiederholen: "}}
    };
    for (const auto &backend : backends) {
        SCOPED_TRACE(backend.domain.constData());
        const auto &translated = backend.translated;
        QMap<QByteArray, QByteArray> messages {{"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"}};
        for (int i = 0; i < 3; ++i)
            messages.insert(backend.messages[i].toUtf8(), translated[i].toUtf8());
        for (auto order : {QDataStream::LittleEndian, QDataStream::BigEndian}) {
            ASSERT_TRUE(writeGettextCatalog(catalogs.path("de", QString::fromLatin1(backend.domain)), messages, order));
            for (const auto &locale : {"de_DE.UTF-8", "de_DE", "de-DE", "de"}) {
                EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(translated[1], QString::fromLatin1(locale)));
                EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(translated[2], QString::fromLatin1(locale)));
                EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt(translated[0], QString::fromLatin1(locale)));
                GreeterPasswordChange change;
                QSignalSpy responses(&change, &GreeterPasswordChange::respond);
                change.start({}, QString::fromLatin1(locale));
                change.prompt(translated[0], true);
                QCoreApplication::processEvents();
                change.submit("old", "newSecret!", "newSecret!", "hint");
                QCoreApplication::processEvents();
                change.prompt(translated[1], true);
                QCoreApplication::processEvents();
                change.prompt(translated[2], true);
                QCoreApplication::processEvents();
                ASSERT_EQ(responses.count(), 3);
                EXPECT_EQ(responses[0][0].toString(), "old");
                EXPECT_EQ(responses[1][0].toString(), "newSecret!");
                EXPECT_EQ(responses[2][0].toString(), "newSecret!");
                EXPECT_TRUE(change.finish(true));
                EXPECT_EQ(change.takeHint(), "hint");
                EXPECT_STREQ(std::setlocale(LC_MESSAGES, nullptr), "C");
                EXPECT_EQ(qgetenv("LANGUAGE"), "en:fr");
            }
        }
        EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Neues Passwort für OTP: ", "de_DE.UTF-8"));
        for (const auto &question : {QString(), QStringLiteral(": "), QStringLiteral("Neues Passwort für OTP: ")}) {
            GreeterPasswordChange change;
            QSignalSpy responses(&change, &GreeterPasswordChange::respond);
            change.start("old", "de_DE.UTF-8");
            change.submit({}, "newSecret!", "newSecret!", {});
            QCoreApplication::processEvents();
            change.prompt(translated[1], true);
            QCoreApplication::processEvents();
            ASSERT_EQ(responses.count(), 1);
            change.prompt(question, true);
            QCoreApplication::processEvents();
            EXPECT_EQ(responses.count(), 1);
            EXPECT_FALSE(change.active());
        }
    }
}

TEST(UT_GreeterPasswordChange, TranslatesPamTokenTemplatesBeforeExpandingUnix)
{
    AuthenticationLocaleCatalogs catalogs;
    ASSERT_TRUE(catalogs.initialize());
    for (const auto placeholder : {QStringLiteral("%s"), QStringLiteral("%1$s")}) {
        const QMap<QByteArray, QByteArray> messages {
            {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
            {"Current %s password: ", (QStringLiteral("Geben Sie das aktuelle ") + placeholder + QStringLiteral(" Passwort ein: ")).toUtf8()},
            {"New %s password: ", (QStringLiteral("Geben Sie ein neues ") + placeholder + QStringLiteral(" Passwort ein: ")).toUtf8()},
            {"Retype new %s password: ", (QStringLiteral("Geben Sie das neue ") + placeholder + QStringLiteral(" Passwort erneut ein: ")).toUtf8()}
        };
        ASSERT_TRUE(writeGettextCatalog(catalogs.path("de", "Linux-PAM"), messages));
        const auto current = QStringLiteral("Geben Sie das aktuelle UNIX Passwort ein: ");
        const auto password = QStringLiteral("Geben Sie ein neues UNIX Passwort ein: ");
        const auto confirmation = QStringLiteral("Geben Sie das neue UNIX Passwort erneut ein: ");
        EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt(current, "de_DE.UTF-8"));
        EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(password, "de_DE.UTF-8"));
        EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt(confirmation, "de_DE.UTF-8"));
        for (bool cached : {false, true}) {
            GreeterPasswordChange change;
            QSignalSpy responses(&change, &GreeterPasswordChange::respond);
            change.start(cached ? QStringLiteral("old") : QString(), "de_DE.UTF-8");
            change.prompt(current, true);
            QCoreApplication::processEvents();
            ASSERT_TRUE(change.active());
            EXPECT_EQ(responses.count(), 0);
            change.submit(cached ? QString() : QStringLiteral("old"), "newSecret!", "newSecret!", {});
            QCoreApplication::processEvents();
            change.prompt(password, true);
            QCoreApplication::processEvents();
            change.prompt(confirmation, true);
            QCoreApplication::processEvents();
            ASSERT_EQ(responses.count(), 3);
            EXPECT_EQ(responses[0][0].toString(), "old");
            EXPECT_EQ(responses[1][0].toString(), "newSecret!");
            EXPECT_EQ(responses[2][0].toString(), "newSecret!");
            EXPECT_TRUE(change.finish(true));
        }
        for (const auto &question : {QStringLiteral("Geben Sie ein neues OTP Passwort ein: "),
                                    QStringLiteral("Geben Sie ein neues %s Passwort ein: ")}) {
            GreeterPasswordChange change;
            QSignalSpy responses(&change, &GreeterPasswordChange::respond);
            change.start("old", "de_DE.UTF-8");
            change.submit({}, "newSecret!", "newSecret!", {});
            QCoreApplication::processEvents();
            change.prompt(question, true);
            QCoreApplication::processEvents();
            EXPECT_FALSE(change.active());
            EXPECT_EQ(responses.count(), 0);
        }
        EXPECT_STREQ(std::setlocale(LC_MESSAGES, nullptr), "C");
        EXPECT_EQ(qgetenv("LANGUAGE"), "en:fr");
    }
}

TEST(UT_GreeterPasswordChange, LocaleModifiersMissingCatalogsAndNewFlowsDoNotReuseTranslations)
{
    AuthenticationLocaleCatalogs catalogs;
    ASSERT_TRUE(catalogs.initialize());
    const QMap<QByteArray, QByteArray> messages {
        {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
        {"New password: ", "Ein anderes neues Passwort: "},
        {"Retype new password: ", "Ein anderes neues Passwort wiederholen: "}
    };
    ASSERT_TRUE(writeGettextCatalog(catalogs.path("de@euro", "Linux-PAM"), messages));
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("Ein anderes neues Passwort: ", "de_AT.UTF-8@euro"));
    EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Ein anderes neues Passwort: ", "de_AT.UTF-8"));
    EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Ein anderes neues Passwort: ", "../../de@euro"));
    EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Ein anderes neues Passwort: ", "missing_LOCALE"));
    EXPECT_TRUE(GreeterPasswordChange::isReplacementPrompt("New password: ", "missing_LOCALE"));

    GreeterPasswordChange change;
    QSignalSpy responses(&change, &GreeterPasswordChange::respond);
    change.start("old", "de_AT.UTF-8@euro");
    change.submit({}, "newSecret!", "newSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("Ein anderes neues Passwort: ", true);
    QCoreApplication::processEvents();
    change.prompt("Ein anderes neues Passwort wiederholen: ", true);
    QCoreApplication::processEvents();
    ASSERT_EQ(responses.count(), 2);
    EXPECT_TRUE(change.finish(true));
    change.start("other-old", "en_US.UTF-8");
    change.submit({}, "differentSecret!", "differentSecret!", {});
    QCoreApplication::processEvents();
    change.prompt("Ein anderes neues Passwort: ", true);
    QCoreApplication::processEvents();
    EXPECT_EQ(responses.count(), 2);
    EXPECT_FALSE(change.active());
}

TEST(UT_GreeterPasswordChange, MalformedAuthenticationLocaleCatalogNeverReceivesCredentials)
{
    AuthenticationLocaleCatalogs catalogs;
    ASSERT_TRUE(catalogs.initialize());
    const auto path = catalogs.path("de", "Linux-PAM");
    const QMap<QByteArray, QByteArray> messages {
        {"", "Content-Type: text/plain; charset=UTF-8\nLanguage: de\n"},
        {"New password: ", "Geben Sie ein neues Passwort ein: "}
    };
    ASSERT_TRUE(writeGettextCatalog(path, messages));
    QFile original(path);
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    const auto valid = original.readAll();
    original.close();
    QList<QByteArray> broken {QByteArray(), valid.left(27), QByteArray(valid.size(), 'x')};
    for (int offset : {4, 8, 12, 16, 36, 40, 52, 56}) {
        auto data = valid;
        qToLittleEndian(quint32(0xffffffff), data.data() + offset);
        broken.append(data);
    }
    auto missingTerminator = valid;
    missingTerminator[missingTerminator.size() - 1] = 'x';
    broken.append(missingTerminator);
    for (const auto &data : broken) {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(data), data.size());
        file.close();
        GreeterPasswordChange change;
        QSignalSpy responses(&change, &GreeterPasswordChange::respond);
        change.start("old", "de_DE.UTF-8");
        change.submit({}, "newSecret!", "newSecret!", {});
        QCoreApplication::processEvents();
        change.prompt("Geben Sie ein neues Passwort ein: ", true);
        QCoreApplication::processEvents();
        EXPECT_EQ(responses.count(), 0);
        EXPECT_FALSE(change.active());
    }
    QFile oversized(path);
    ASSERT_TRUE(oversized.open(QIODevice::WriteOnly));
    ASSERT_EQ(oversized.write(valid), valid.size());
    ASSERT_TRUE(oversized.resize(32 * 1024 * 1024));
    oversized.close();
    EXPECT_FALSE(GreeterPasswordChange::isReplacementPrompt("Geben Sie ein neues Passwort ein: ", "de_DE.UTF-8"));
}
