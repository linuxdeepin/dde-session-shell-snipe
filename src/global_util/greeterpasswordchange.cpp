// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeterpasswordchange.h"

#include <QFile>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <initializer_list>
#include <libintl.h>

namespace {
QStringList catalogLocales(QString locale)
{
    locale = locale.trimmed();
    if (locale.isEmpty() || locale.size() > 128 || locale.contains(QStringLiteral("..")))
        return {};
    for (const auto c : locale) {
        if (c.unicode() > 0x7f || (!c.isLetterOrNumber() && c != QLatin1Char('_')
            && c != QLatin1Char('-') && c != QLatin1Char('.') && c != QLatin1Char('@')))
            return {};
    }
    const auto modifier = locale.section(QLatin1Char('@'), 1);
    auto base = locale.section(QLatin1Char('@'), 0, 0).section(QLatin1Char('.'), 0, 0);
    base.replace(QLatin1Char('-'), QLatin1Char('_'));
    if (base.isEmpty())
        return {};
    const auto language = base.section(QLatin1Char('_'), 0, 0);
    QStringList names {locale};
    if (!modifier.isEmpty())
        names << base + QLatin1Char('@') + modifier << language + QLatin1Char('@') + modifier;
    names << base << language;
    names.removeDuplicates();
    return names;
}

// gettext has no explicit-locale lookup; even a thread-local LC_MESSAGES can
// be overridden by the process's LANGUAGE. Read only singular prompt entries
// from the selected catalogs, without changing process/thread language state.
QString catalogTranslation(const QByteArray &data, const QByteArray &message)
{
    if (data.size() < 28)
        return {};
    const auto magic = qFromLittleEndian<quint32>(data.constData());
    if (magic != 0x950412de && magic != 0xde120495)
        return {};
    const auto word = [&](quint32 offset) {
        return magic == 0x950412de ? qFromLittleEndian<quint32>(data.constData() + offset)
                                   : qFromBigEndian<quint32>(data.constData() + offset);
    };
    if ((word(4) >> 16) > 1)
        return {};
    const auto count = word(8);
    const auto originals = word(12);
    const auto translations = word(16);
    const auto size = quint64(data.size());
    if (originals > size || translations > size
        || count > (size - originals) / 8 || count > (size - translations) / 8)
        return {};
    const auto entry = [&](quint32 table, quint32 index, QByteArray &text) {
        const auto length = word(table + index * 8);
        const auto offset = word(table + index * 8 + 4);
        if (offset >= size || length >= size - offset || data.at(offset + length) != '\0')
            return false;
        text = data.mid(offset, length);
        return true;
    };
    quint32 first = 0, last = count;
    while (first < last) {
        const auto index = first + (last - first) / 2;
        QByteArray original;
        if (!entry(originals, index, original))
            return {};
        const auto comparison = original.compare(message);
        if (comparison < 0) {
            first = index + 1;
        } else if (comparison > 0) {
            last = index;
        } else {
            QByteArray translated;
            if (!entry(translations, index, translated) || translated.contains('\0'))
                return {};
            // The supported PAM catalogs, like the LightDM prompt transport,
            // encode these singular messages as UTF-8.
            return QString::fromUtf8(translated);
        }
    }
    return {};
}
}

GreeterPasswordChange::GreeterPasswordChange(QObject *parent)
    : QObject(parent)
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(60 * 1000);
    m_operationTimer.setSingleShot(true);
    m_operationTimer.setInterval(60 * 1000);
    m_promptTimer.setSingleShot(true);
    m_promptTimer.setInterval(0);
    connect(&m_idleTimer, &QTimer::timeout, this, &GreeterPasswordChange::cancel);
    connect(&m_operationTimer, &QTimer::timeout, this, &GreeterPasswordChange::cancel);
    connect(&m_promptTimer, &QTimer::timeout, this, &GreeterPasswordChange::respondToPrompts);
}

GreeterPasswordChange::Input GreeterPasswordChange::classifyPrompt(const QString &text, const QString &locale)
{
    const auto canonical = [](QString prompt) {
        prompt = prompt.simplified().toCaseFolded();
        prompt.remove(QLatin1Char(' '));
        while (prompt.endsWith(QLatin1Char(':')) || prompt.endsWith(QChar(0xff1a)))
            prompt.chop(1);
        // Both forms occur in PAM stacks, independently of the greeter locale.
        prompt.replace(QStringLiteral("新的"), QStringLiteral("新"));
        return prompt;
    };
    const auto prompt = canonical(text);
    if (prompt.isEmpty())
        return Input::None;
    const auto locales = catalogLocales(locale);
    QHash<QString, QByteArray> catalogs;
    const auto matches = [&](std::initializer_list<const char *> messages, const char *domain = "Linux-PAM",
                             const char *tokenType = nullptr) {
        const auto expanded = [&](QString message) {
            // PAM translates the printf template before inserting authtok_type.
            // Expand only the known type, without interpreting catalog text as
            // a printf format string. Gettext also permits positional %1$s.
            if (tokenType) {
                message.replace(QStringLiteral("%1$s"), QString::fromLatin1(tokenType));
                message.replace(QStringLiteral("%s"), QString::fromLatin1(tokenType));
            }
            return canonical(message);
        };
        for (const auto message : messages) {
            if (prompt == expanded(QString::fromUtf8(message)))
                return true;
            for (const auto &name : locales) {
                const auto path = QString::fromUtf8(bindtextdomain(domain, nullptr)) + QLatin1Char('/')
                    + name + QStringLiteral("/LC_MESSAGES/") + QString::fromLatin1(domain) + QStringLiteral(".mo");
                auto catalog = catalogs.constFind(path);
                if (catalog == catalogs.cend()) {
                    QFile file(path);
                    QByteArray data;
                    constexpr qint64 maxCatalogSize = 16 * 1024 * 1024;
                    if (file.open(QIODevice::ReadOnly) && file.size() <= maxCatalogSize) {
                        data = file.read(maxCatalogSize + 1);
                        if (data.size() > maxCatalogSize)
                            data.clear();
                    }
                    catalog = catalogs.insert(path, data);
                }
                if (prompt == expanded(catalogTranslation(catalog.value(), QByteArray(message))))
                    return true;
            }
            // Keep the daemon/system language and existing literal aliases as
            // fallbacks: LightDM does not attach a locale to prompt messages.
            if (prompt == expanded(QString::fromUtf8(dgettext(domain, message))))
                return true;
        }
        return false;
    };
    if (matches({"Retype new password: ", "Retype new UNIX password: ",
                 "Repeat new password: ", "Confirm new password: ",
                 "重新输入新密码：", "再次输入新密码：", "确认新密码：",
                 "重新输入新口令：", "再次输入新口令：", "确认新口令："})
        || matches({"Retype new %s password: "}, "Linux-PAM", "UNIX")
        || matches({"Reenter new Password: "}, "sssd")
        || matches({"Retype new NT password: ", "重键入新 NT 密码："}, "pam_winbind"))
        return Input::Confirmation;
    if (matches({"New password: ", "New UNIX password: ", "New authentication token: ",
                 "新密码：", "请输入新密码：", "新 UNIX 密码：", "新口令："})
        || matches({"New %s password: "}, "Linux-PAM", "UNIX")
        || matches({"New Password: "}, "sssd")
        || matches({"Enter new NT password: ", "输入新 NT 密码："}, "pam_winbind"))
        return Input::New;
    if (matches({"Password: ", "Current password: ", "Old password: ",
                 "(current) UNIX password: ", "Current UNIX password: ",
                 "密码：", "口令：", "当前密码：", "当前口令：", "旧密码：", "旧口令："})
        || matches({"Current %s password: "}, "Linux-PAM", "UNIX")
        || matches({"Password: ", "Current Password: "}, "sssd")
        || matches({"(current) NT password: ", "（当前）NT 密码："}, "pam_winbind"))
        return Input::Current;
    return Input::None;
}

bool GreeterPasswordChange::isReplacementPrompt(const QString &text, const QString &locale)
{
    const auto input = classifyPrompt(text, locale);
    return input == Input::New || input == Input::Confirmation;
}

bool GreeterPasswordChange::isExpiryMessage(const QString &text)
{
    const QString message = text.toLower();
    return (message.contains(QStringLiteral("password")) && message.contains(QStringLiteral("expired")))
        || message.contains(QStringLiteral("密码已过期")) || message.contains(QStringLiteral("口令已过期"));
}

void GreeterPasswordChange::start(const QString &current, const QString &locale, bool passwordless)
{
    if (m_active)
        return;
    reset();
    m_locale = locale;
    m_active = true;
    m_passwordless = passwordless;
    m_current = passwordless ? QString() : current;
    m_currentRequired = !passwordless && m_current.isEmpty();
    m_idleTimer.start();
    emit formChanged(currentRequired(), false, {});
}

void GreeterPasswordChange::prompt(const QString &text, bool secret)
{
    if (!m_active)
        return;
    // Informational PAM messages are never password requests. Unknown question
    // prompts must not receive cached credentials (e.g. OTP or consent).
    if (!secret) {
        cancel();
        return;
    }
    const auto input = classifyPrompt(text, m_locale);
    // Never guess a credential's role from the expected order. PAM may skip
    // the current password or ask for another factor; unknown prompts cancel.
    if (input == Input::None) {
        cancel();
        return;
    }

    // LightDM emits every question in a PAM batch synchronously. Retain all
    // roles and finish dispatching the batch before any cached response.
    m_pending.enqueue(input);
    m_promptTimer.start();
}

void GreeterPasswordChange::message(bool error)
{
    if (!m_active || !m_submitted || !error)
        return;
    if (m_replacementSent) {
        // PAM output can contain passwords. Never forward it to the UI/log.
        m_errorReceived = true;
        emit formChanged(currentRequired(), m_busy, tr("Password change failed. Please try again."));
    } else if (m_expected == Input::New) {
        // An error after a current-password response can be an expiry notice.
        // Advancing to New confirms that authentication passed; asking Current
        // again means the old credential was rejected and must not be replayed.
        m_currentErrorReceived = true;
    }
}

void GreeterPasswordChange::submit(const QString &current, const QString &password,
                                 const QString &confirmation, const QString &hint)
{
    if (!m_active || m_busy)
        return;
    const QString oldPassword = currentRequired() ? current : m_current;
    QString error;
    if ((!m_passwordless && oldPassword.isEmpty()) || password.isEmpty())
        error = tr("Password cannot be empty");
    else if (password != confirmation)
        error = tr("The two password inputs are inconsistent.");
    else if (password == oldPassword)
        error = tr("New password must differ from the current password");
    else if (std::any_of(password.cbegin(), password.cend(), [](QChar c) {
                 return c.unicode() < 0x20 || c.unicode() > 0x7e;
             }))
        error = tr("Use English letters, numbers, half-width symbols and spaces for the new password.");
    else if (hint.size() > 14 || hint.contains(password)
             || (!oldPassword.isEmpty() && hint.contains(oldPassword)))
        error = tr("The hint is visible to all users. Do not include the password here.");
    if (!error.isEmpty()) {
        emit formChanged(currentRequired(), false, error);
        return;
    }
    m_current = oldPassword;
    m_password = password;
    m_hint = hint;
    m_busy = m_submitted = true;
    m_idleTimer.stop();
    m_operationTimer.start();
    emit formChanged(currentRequired(), true, {});
    if (m_active && m_submitted && !m_pending.isEmpty())
        m_promptTimer.start();
}

void GreeterPasswordChange::respondToPrompts()
{
    if (!m_active || !m_submitted || m_pending.isEmpty())
        return;
    const auto first = m_pending.head();
    if (m_errorReceived || m_confirmationSent
        || (m_replacementSent && (first == Input::New || first == Input::Current))
        || (m_currentErrorReceived && first == Input::Current)) {
        // Preserve the complete retry batch for the next Save. A confirmation
        // retry still belongs to the replacement already sent to PAM.
        const bool confirmationRetry = first == Input::Confirmation && m_expected == Input::Confirmation;
        ++m_generation;
        m_password.fill(QChar(0));
        m_password.clear();
        m_hint.clear();
        m_busy = m_submitted = m_confirmationSent = m_errorReceived = m_currentErrorReceived = false;
        m_replacementSent = confirmationRetry;
        m_expected = confirmationRetry ? Input::Confirmation : Input::Current;
        if (first == Input::Current) {
            m_current.fill(QChar(0));
            m_current.clear();
        }
        m_currentRequired = !m_passwordless && m_current.isEmpty();
        m_operationTimer.stop();
        m_idleTimer.start();
        emit formChanged(currentRequired(), false, tr("Password change failed. Please try again."));
        return;
    }

    // Validate the whole batch before emitting a secret. Current may be asked
    // more than once, or skipped; New must precede Confirmation and nothing
    // can follow Confirmation in the same batch.
    auto expected = m_expected;
    bool confirmed = false;
    for (const auto input : m_pending) {
        if (confirmed || (expected == Input::Confirmation && input != Input::Confirmation)
            || (input == Input::Confirmation && expected != Input::Confirmation)) {
            cancel();
            return;
        }
        if (input == Input::Current)
            expected = Input::New;
        else if (input == Input::New)
            expected = Input::Confirmation;
        else
            confirmed = true;
    }

    const auto generation = m_generation;
    QPointer<GreeterPasswordChange> self(this);
    while (!m_pending.isEmpty()) {
        const auto input = m_pending.dequeue();
        QString response = input == Input::Current ? m_current : m_password;
        m_errorReceived = m_currentErrorReceived = false;
        if (input == Input::Current) {
            m_expected = Input::New;
        } else if (input == Input::New) {
            m_expected = Input::Confirmation;
            m_replacementSent = true;
        } else {
            m_confirmationSent = true;
            clearSecrets();
        }
        emit respond(response);
        response.fill(QChar(0));
        if (!self || self->m_generation != generation)
            return;
        // A callback may append prompts or deliver a rejection. Recheck that
        // batch on the next event turn before sending another response.
        if (m_promptTimer.isActive() || m_errorReceived || m_currentErrorReceived) {
            if (!m_pending.isEmpty())
                m_promptTimer.start();
            return;
        }
    }
}

void GreeterPasswordChange::resetIdleTimer()
{
    if (m_active && !m_busy)
        m_idleTimer.start();
}

void GreeterPasswordChange::clearSecrets()
{
    m_current.fill(QChar(0));
    m_current.clear();
    m_password.fill(QChar(0));
    m_password.clear();
}

void GreeterPasswordChange::reset()
{
    m_idleTimer.stop();
    m_operationTimer.stop();
    m_promptTimer.stop();
    ++m_generation;
    m_active = m_busy = m_submitted = m_confirmationSent = m_replacementSent = m_currentErrorReceived = m_errorReceived = false;
    m_currentRequired = true;
    m_passwordless = false;
    m_pending.clear();
    m_expected = Input::Current;
    clearSecrets();
    m_hint.clear();
    m_locale.clear();
    emit inputsCleared();
}

void GreeterPasswordChange::cancel()
{
    if (!m_active)
        return;
    reset();
    emit canceled();
}

bool GreeterPasswordChange::finish(bool authenticated)
{
    const bool accepted = m_active && m_submitted && m_confirmationSent && authenticated;
    const QString hint = accepted ? m_hint : QString();
    reset();
    m_hint = hint;
    return accepted;
}

QString GreeterPasswordChange::takeHint()
{
    const QString hint = m_hint;
    m_hint.clear();
    return hint;
}
