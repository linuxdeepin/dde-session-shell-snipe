// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQueue>
#include <QTimer>

// Adapts the shared form to an existing LightDM password-change conversation.
// Only LightDM's authenticationComplete can authorize starting a session.
class GreeterPasswordChange : public QObject
{
    Q_OBJECT
public:
    explicit GreeterPasswordChange(QObject *parent = nullptr);
    bool active() const { return m_active; }
    bool busy() const { return m_busy; }
    bool currentRequired() const { return m_currentRequired; }
    static bool isReplacementPrompt(const QString &text, const QString &locale = {});
    static bool isExpiryMessage(const QString &text);
    // Passwordless accounts have a valid empty credential, rather than a
    // missing cached password. Do not infer this from current.isEmpty().
    void start(const QString &current = {}, const QString &locale = {}, bool passwordless = false);
    // Collect a complete PAM batch and respond on the next event turn.
    void prompt(const QString &text, bool secret);
    void message(bool error);
    void submit(const QString &current, const QString &password,
                const QString &confirmation, const QString &hint);
    void resetIdleTimer();
    void cancel();
    void reset();
    // Returns true only after both replacement-password responses were sent.
    bool finish(bool authenticated);
    QString takeHint();

signals:
    void formChanged(bool currentRequired, bool busy, const QString &error);
    void inputsCleared();
    void respond(const QString &password);
    void canceled();

private:
    enum class Input { None, Current, New, Confirmation };
    static Input classifyPrompt(const QString &text, const QString &locale);
    void respondToPrompts();
    void clearSecrets();
    bool m_currentRequired = true;
    bool m_passwordless = false;
    bool m_active = false;
    bool m_busy = false;
    bool m_submitted = false;
    bool m_confirmationSent = false;
    bool m_replacementSent = false;
    bool m_currentErrorReceived = false;
    bool m_errorReceived = false;
    QQueue<Input> m_pending;
    Input m_expected = Input::Current;
    quint64 m_generation = 0;

    QString m_current;
    QString m_password;
    QString m_hint;
    QString m_locale;
    QTimer m_idleTimer;
    QTimer m_operationTimer;
    QTimer m_promptTimer;
};
