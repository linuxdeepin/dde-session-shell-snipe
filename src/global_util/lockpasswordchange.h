// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QPointer>
#include <functional>

class QThread;

// Owned by LockWorker, never instantiated by the greeter.
class LockPasswordChange : public QObject
{
    Q_OBJECT
public:
    using AccountChecker = std::function<int(const QString &, const QString &)>;
    explicit LockPasswordChange(QObject *parent = nullptr,
                                AccountChecker checker = {},
                                QString passwdProgram = QStringLiteral("/usr/bin/passwd"));
    ~LockPasswordChange() override;

    bool isActive() const { return m_stage != Idle; }
    bool isCheckingExpiry() const { return m_stage == CheckingExpiry; }
    bool acceptsInput() const { return m_stage == CurrentPassword || m_stage == NewPassword || m_stage == ConfirmPassword; }
    void begin(const QString &account, const QString &oldPassword,
               bool alreadyExpired, bool skipCurrentPassword, bool passwordless = false,
               const QString &accountPath = {});
    void submit(const QString &password);
    void submitPasswords(const QString &currentPassword, const QString &newPassword,
                         const QString &confirmation, const QString &hint = {});
    void restartAtCurrentPassword();
    void cancel(const QString &reason = {});
    void refreshPrompt();
    void resetIdleTimer();

signals:
    void activeChanged(bool active);
    void promptChanged(const QString &prompt, const QString &error, bool busy);
    void formChanged(bool currentRequired, bool checkingOnly, bool busy, const QString &error);
    void inputsCleared();
    void completed(bool passwordChanged);
    void canceled(const QString &reason);

private:
    enum Stage { Idle, CheckingExpiry, CurrentPassword, NewPassword, ConfirmPassword, ChangingPassword };
    static int checkAccount(const QString &account, const QString &password);
    static int checkAccountExpiry(const QString &accountPath);
    void startAccountCheck(const QString &accountPath = {});
    static bool validPassword(const QString &password);
    bool validateNewPassword(const QString &password, const QString &oldPassword);
    void expiryChecked(int result, quint64 generation, bool expiryOnly = false);
    void startProcess();
    void stopProcess();
    void finish(bool changed);
    void fail(const QString &message);
    void showPrompt(const QString &error = {});
    void appendOutput(const QByteArray &output);
    QString sanitizedOutput() const;
    void clearPasswords();

    Stage m_stage = Idle;
    quint64 m_generation = 0;
    QString m_account;
    QString m_oldPassword;
    QString m_newPassword;
    bool m_skipCurrentPassword = false;
    bool m_passwordless = false;
    bool m_checkAfterCurrentPassword = false;
    QProcess *m_process = nullptr;
    QPointer<QThread> m_checkThread;
    QByteArray m_output;
    bool m_outputTruncated = false;
    QTimer m_timer;
    AccountChecker m_checker;
    const QString m_passwdProgram;
};
