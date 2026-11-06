// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockpasswordchange.h"
#include "dbusconstant.h"
#include "userinfo.h"

#include <QDBusConnection>
#include <QDBusReply>
#include <QList>
#include <QProcessEnvironment>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <security/pam_appl.h>

namespace {
constexpr int CheckTimeout = 15000;
constexpr int IdleTimeout = 60 * 1000;
constexpr int ProcessTimeout = 30000;
constexpr int OutputLimit = 8192;

struct AccountConversation {
    QByteArray password;
    bool passwordSent = false;
};

int accountConversation(int count, const pam_message **messages, pam_response **responses, void *data)
{
    if (count <= 0 || !messages || !responses || !data) {
        return PAM_CONV_ERR;
    }
    *responses = nullptr;
    auto *conversation = static_cast<AccountConversation *>(data);
    bool needsPassword = false;
    for (int i = 0; i < count; ++i) {
        if (!messages[i]) {
            return PAM_CONV_ERR;
        }
        if (messages[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            if (conversation->passwordSent || needsPassword) {
                return PAM_CONV_ERR;
            }
            needsPassword = true;
        } else if (messages[i]->msg_style != PAM_TEXT_INFO && messages[i]->msg_style != PAM_ERROR_MSG) {
            return PAM_CONV_ERR;
        }
    }
    *responses = static_cast<pam_response *>(std::calloc(count, sizeof(pam_response)));
    if (!*responses) {
        return PAM_BUF_ERR;
    }
    for (int i = 0; i < count; ++i) {
        if (messages[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            const auto size = conversation->password.size() + 1;
            (*responses)[i].resp = static_cast<char *>(std::malloc(size));
            if (!(*responses)[i].resp) {
                std::free(*responses);
                *responses = nullptr;
                return PAM_BUF_ERR;
            }
            std::memcpy((*responses)[i].resp, conversation->password.constData(), size);
            conversation->passwordSent = true;
        }
    }
    return PAM_SUCCESS;
}
}

LockPasswordChange::LockPasswordChange(QObject *parent, AccountChecker checker, QString passwdProgram)
    : QObject(parent)
    , m_checker(checker ? std::move(checker) : checkAccount)
    , m_passwdProgram(std::move(passwdProgram))
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_stage == ChangingPassword) {
            stopProcess();
            fail(tr("Password change timed out. Please try again."));
        } else if (m_stage == CheckingExpiry) {
            cancel(tr("Unable to check password expiration. Please authenticate again."));
        } else {
            cancel();
        }
    });
}

LockPasswordChange::~LockPasswordChange()
{
    stopProcess();
    clearPasswords();
}

int LockPasswordChange::checkAccount(const QString &account, const QString &password)
{
    const QByteArray name = account.toUtf8();
    pam_handle_t *handle = nullptr;
    AccountConversation data {password.toUtf8()};
    const pam_conv conversation = {accountConversation, &data};
    // Reuse the installed login account policy (including SSSD's service/GPO
    // mapping) without changing the legacy lock PAM stack or greeter flow.
    int result = pam_start("lightdm", name.constData(), &conversation, &handle);
    if (result == PAM_SUCCESS) {
        // SSSD may only remember password expiry after authentication in this
        // same handle; a standalone account check can incorrectly return success.
        result = pam_authenticate(handle, PAM_SILENT);
        if (result == PAM_SUCCESS) {
            result = pam_acct_mgmt(handle, PAM_SILENT);
        }
    }
    if (handle) {
        pam_end(handle, result);
    }
    data.password.fill('\0');
    return result;
}

bool LockPasswordChange::validPassword(const QString &password)
{
    // passwd consumes one response per line; never turn password data into extra responses.
    return !password.isEmpty() && !password.contains(QChar::Null)
        && !password.contains('\n') && !password.contains('\r');
}

int LockPasswordChange::checkAccountExpiry(const QString &accountPath)
{
    const auto request = QDBusMessage::createMethodCall(DSS_DBUS::accountsService, accountPath,
                                                       DSS_DBUS::accountsUserInterface,
                                                       QStringLiteral("PasswordExpiredInfo"));
    const auto message = QDBusConnection::systemBus().call(request, QDBus::Block, 3000);
    const QDBusReply<int> reply(message);
    // Cached/default states cannot establish that a password is still valid.
    // A failed or unrecognized account query falls back to credential-based PAM.
    if (!reply.isValid() || message.arguments().size() != 2) {
        return PAM_CRED_INSUFFICIENT;
    }
    switch (reply.value()) {
    case User::ExpiredNormal:
    case User::ExpiredSoon:
        return PAM_SUCCESS;
    case User::ExpiredAlready:
        return PAM_NEW_AUTHTOK_REQD;
    default:
        return PAM_CRED_INSUFFICIENT;
    }
}

void LockPasswordChange::begin(const QString &account, const QString &oldPassword,
                               bool alreadyExpired, bool skipCurrentPassword, bool passwordless,
                               const QString &accountPath)
{
    if (isActive() || account.isEmpty()) {
        return;
    }
    m_account = account;
    m_oldPassword = !passwordless && validPassword(oldPassword) ? oldPassword : QString();
    m_passwordless = passwordless;
    m_skipCurrentPassword = skipCurrentPassword || passwordless;
    m_stage = CheckingExpiry;
    ++m_generation;
    m_checkAfterCurrentPassword = !alreadyExpired && m_oldPassword.isEmpty() && !passwordless;
    emit activeChanged(true);
    if (alreadyExpired && !passwordless) {
        const auto generation = m_generation;
        showPrompt();
        QTimer::singleShot(0, this, [this, generation] { expiryChecked(PAM_NEW_AUTHTOK_REQD, generation); });
        return;
    }
    if (m_checkAfterCurrentPassword && accountPath.isEmpty()) {
        m_stage = CurrentPassword;
        resetIdleTimer();
        showPrompt();
        return;
    }
    startAccountCheck(m_checkAfterCurrentPassword ? accountPath : QString());
}

void LockPasswordChange::startAccountCheck(const QString &accountPath)
{
    m_stage = CheckingExpiry;
    const auto generation = m_generation;
    showPrompt();
    m_timer.start(CheckTimeout);
    if (m_checkThread && m_checkThread->isRunning()) {
        cancel(tr("A password expiration check is still running. Please try again shortly."));
        return;
    }

    // PAM/NSS can contact a domain controller. Keep it off the UI thread and
    // ignore late replies after cancellation or a user switch.
    const auto result = std::make_shared<std::atomic<int>>(PAM_SYSTEM_ERR);
    const auto checker = m_checker;
    auto *thread = QThread::create([checker, account = m_account, password = m_oldPassword, accountPath, result] {
        result->store(accountPath.isEmpty() ? checker(account, password) : checkAccountExpiry(accountPath));
    });
    m_checkThread = thread;
    connect(thread, &QThread::finished, this, [this, result, generation, accountPath] {
        expiryChecked(result->load(), generation, !accountPath.isEmpty());
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void LockPasswordChange::expiryChecked(int result, quint64 generation, bool expiryOnly)
{
    if (generation != m_generation || m_stage != CheckingExpiry) {
        return;
    }
    m_timer.stop();
    if (result == PAM_SUCCESS) {
        finish(false);
    } else if (result == PAM_NEW_AUTHTOK_REQD || result == PAM_AUTHTOK_EXPIRED) {
        m_checkAfterCurrentPassword = false;
        m_stage = !m_skipCurrentPassword && m_oldPassword.isEmpty() ? CurrentPassword : NewPassword;
        resetIdleTimer();
        showPrompt();
    } else if (expiryOnly && result == PAM_CRED_INSUFFICIENT) {
        m_checkAfterCurrentPassword = true;
        m_stage = CurrentPassword;
        resetIdleTimer();
        showPrompt();
    } else {
        // Account lookup/access failures must not be interpreted as "not expired".
        cancel(tr("Unable to check password expiration or account access. Please authenticate again."));
    }
}

void LockPasswordChange::submit(const QString &password)
{
    if (m_stage != CurrentPassword && m_stage != NewPassword && m_stage != ConfirmPassword) {
        return;
    }
    resetIdleTimer();
    if (!validPassword(password)) {
        showPrompt(tr("The password cannot be empty or contain a line break."));
        return;
    }
    if (m_stage == CurrentPassword) {
        m_oldPassword = password;
        if (m_checkAfterCurrentPassword) {
            startAccountCheck();
            return;
        }
        m_stage = NewPassword;
    } else if (m_stage == NewPassword) {
        // Normal PAM and password-factor inputs accept printable ASCII only.
        // Do not commit a replacement the user cannot enter on the next login.
        if (!validateNewPassword(password, m_oldPassword)) {
            return;
        }
        // Leave AD password policy to the passwd PAM stack/domain server.
        m_newPassword = password;
        m_stage = ConfirmPassword;
    } else if (password != m_newPassword) {
        m_newPassword.clear();
        m_stage = NewPassword;
        showPrompt(tr("The two password inputs are inconsistent."));
        return;
    } else {
        startProcess();
        return;
    }
    showPrompt();
}

void LockPasswordChange::submitPasswords(const QString &currentPassword, const QString &newPassword,
                                        const QString &confirmation, const QString &hint)
{
    if (m_stage != CurrentPassword && m_stage != NewPassword && m_stage != ConfirmPassword) {
        return;
    }
    resetIdleTimer();
    if (m_stage == CurrentPassword && m_checkAfterCurrentPassword) {
        submit(currentPassword);
        return;
    }
    const auto oldPassword = m_stage == CurrentPassword ? currentPassword : m_oldPassword;
    if ((!m_skipCurrentPassword && !validPassword(oldPassword)) || !validPassword(newPassword)) {
        showPrompt(tr("The password cannot be empty or contain a line break."));
        return;
    }
    if (!validateNewPassword(newPassword, oldPassword)) {
        return;
    }
    if (newPassword != confirmation) {
        showPrompt(tr("The two password inputs are inconsistent."));
        return;
    }
    if (hint.contains(newPassword) || (!oldPassword.isEmpty() && hint.contains(oldPassword))) {
        showPrompt(tr("The hint is visible to all users. Do not include the password here."));
        return;
    }
    if (m_stage == CurrentPassword) {
        submit(currentPassword);
    }
    // Keep validation and passwd execution in the existing controller. Submit
    // both new-password fields together without exposing an intermediate page.
    m_stage = NewPassword;
    submit(newPassword);
    if (m_stage == ConfirmPassword) {
        submit(confirmation);
    }
}

bool LockPasswordChange::validateNewPassword(const QString &password, const QString &oldPassword)
{
    if (std::any_of(password.cbegin(), password.cend(), [](QChar character) {
            return character.unicode() < 0x20 || character.unicode() > 0x7e;
        })) {
        showPrompt(tr("Use English letters, numbers, half-width symbols and spaces for the new password."));
        return false;
    }
    if (password == oldPassword) {
        showPrompt(tr("New password must differ from the current password"));
        return false;
    }
    return true;
}

void LockPasswordChange::restartAtCurrentPassword()
{
    if (!isActive()) {
        return;
    }
    if (m_passwordless) {
        // There is no current password to re-enter. Return to normal lock
        // authentication; a new user action must start the next PAM check.
        cancel();
        return;
    }
    m_timer.stop();
    // Sleep invalidates busy operations too: a late PAM/passwd success must
    // never unlock with pre-sleep authentication. PAM threads finish normally;
    // their generation is stale, while passwd callbacks are disconnected.
    ++m_generation;
    stopProcess();
    clearPasswords();
    m_stage = CurrentPassword;
    m_checkAfterCurrentPassword = true;
    emit inputsCleared();
    resetIdleTimer();
    showPrompt();
}

void LockPasswordChange::startProcess()
{
    m_stage = ChangingPassword;
    m_output.clear();
    m_outputTruncated = false;
    showPrompt();
    m_timer.start(ProcessTimeout);
    auto *process = new QProcess(this);
    m_process = process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::started, this, [this, process] {
        if (m_process != process) {
            return;
        }
        QByteArray responses;
        if (!m_skipCurrentPassword) {
            responses = m_oldPassword.toUtf8() + '\n';
        }
        responses += m_newPassword.toUtf8() + '\n' + m_newPassword.toUtf8() + '\n';
        process->write(responses);
        responses.fill('\0');
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyRead, this, [this, process] {
        if (m_process == process) {
            appendOutput(process->readAll());
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus status) {
        if (m_process != process) {
            return;
        }
        m_timer.stop();
        appendOutput(process->readAll());
        const auto error = sanitizedOutput();
        stopProcess();
        if (status == QProcess::NormalExit && exitCode == 0) {
            finish(true);
        } else {
            // A failed current-password check also needs a chance to correct the old password.
            m_oldPassword.clear();
            fail(error.isEmpty() ? tr("Password change failed. Please try again.") : error);
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        if (m_process != process) {
            return;
        }
        stopProcess();
        fail(tr("Unable to run the password change process. Please try again."));
    });
    process->start(m_passwdProgram, {QStringLiteral("-q")});
}

void LockPasswordChange::appendOutput(const QByteArray &output)
{
    const auto remaining = qMax(0, OutputLimit - int(m_output.size()));
    m_outputTruncated = m_outputTruncated || output.size() > remaining;
    m_output += output.left(remaining);
}

QString LockPasswordChange::sanitizedOutput() const
{
    // Truncation can split either secret (including a UTF-8 code point).
    // A partial secret cannot be safely redacted by full-string matching.
    if (m_outputTruncated)
        return tr("Password change failed. Please try again.");
    const auto original = QString::fromUtf8(m_output);
    QList<QPair<qsizetype, qsizetype>> matches;
    // Find both passwords in the original text, including overlapping matches.
    // Replacing one first can expose the other or modify a redaction marker.
    for (const auto &password : {m_oldPassword, m_newPassword}) {
        if (!password.isEmpty()) {
            qsizetype start = original.indexOf(password);
            while (start >= 0) {
                matches.append({start, start + password.size()});
                start = original.indexOf(password, start + 1);
            }
        }
    }
    std::sort(matches.begin(), matches.end());
    QString output;
    qsizetype position = 0;
    for (auto it = matches.cbegin(); it != matches.cend();) {
        const auto start = it->first;
        auto end = it->second;
        ++it;
        while (it != matches.cend() && it->first <= end) {
            end = qMax(end, it->second);
            ++it;
        }
        output += original.mid(position, start - position) + QStringLiteral("[redacted]");
        position = end;
    }
    output += original.mid(position);
    output.remove(QStringLiteral("Current password:"), Qt::CaseInsensitive);
    output.remove(QStringLiteral("Retype new password:"), Qt::CaseInsensitive);
    output.remove(QStringLiteral("New password:"), Qt::CaseInsensitive);
    return output.trimmed();
}

void LockPasswordChange::stopProcess()
{
    if (!m_process) {
        return;
    }
    auto *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
    }
    process->deleteLater();
}

void LockPasswordChange::clearPasswords()
{
    m_oldPassword.clear();
    m_newPassword.clear();
    m_output.fill('\0');
    m_output.clear();
    m_outputTruncated = false;
}

void LockPasswordChange::finish(bool changed)
{
    m_timer.stop();
    m_stage = Idle;
    ++m_generation;
    clearPasswords();
    m_account.clear();
    emit activeChanged(false);
    emit completed(changed);
}

void LockPasswordChange::cancel(const QString &reason)
{
    if (!isActive()) {
        return;
    }
    m_timer.stop();
    stopProcess();
    m_stage = Idle;
    ++m_generation;
    clearPasswords();
    m_account.clear();
    emit activeChanged(false);
    emit canceled(reason);
}

void LockPasswordChange::fail(const QString &message)
{
    m_newPassword.clear();
    m_output.fill('\0');
    m_output.clear();
    m_outputTruncated = false;
    m_stage = !m_skipCurrentPassword && m_oldPassword.isEmpty() ? CurrentPassword : NewPassword;
    emit inputsCleared();
    resetIdleTimer();
    showPrompt(message);
}

void LockPasswordChange::refreshPrompt()
{
    if (isActive()) {
        showPrompt();
    }
}

void LockPasswordChange::resetIdleTimer()
{
    if (m_stage == CurrentPassword || m_stage == NewPassword || m_stage == ConfirmPassword) {
        m_timer.start(IdleTimeout);
    }
}

void LockPasswordChange::showPrompt(const QString &error)
{
    QString prompt;
    switch (m_stage) {
    case CheckingExpiry: prompt = tr("Checking password expiration..."); break;
    case CurrentPassword: prompt = tr("Please enter the old password."); break;
    case NewPassword: prompt = tr("Please enter the new password."); break;
    case ConfirmPassword: prompt = tr("Confirm the new password."); break;
    case ChangingPassword: prompt = tr("Changing password..."); break;
    case Idle: return;
    }
    emit promptChanged(prompt, error, m_stage == CheckingExpiry || m_stage == ChangingPassword);
    emit formChanged(m_stage == CurrentPassword,
                     m_checkAfterCurrentPassword || m_stage == CheckingExpiry,
                     m_stage == CheckingExpiry || m_stage == ChangingPassword, error);
}
