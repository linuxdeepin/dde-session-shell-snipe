// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockpasswordchangewidget.h"
#include "userinfo.h"
#include "useravatar.h"
#include "user_name_widget.h"
#include "dconfig_helper.h"

#include <deepin_pw_check.h>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE
using dcc::widgets::SecurityLevelItem;

LockPasswordChangeWidget::LockPasswordChangeWidget(QWidget *parent)
    : QWidget(parent)
    , m_avatar(new UserAvatar(this))
    , m_userName(new UserNameWidget(false, true, this))
    , m_info(new DLabel(this))
    , m_error(new QPlainTextEdit(this))
    , m_current(new PasswordInputWidget(this))
    , m_password(new PasswordInputWidget(this))
    , m_confirmation(new PasswordInputWidget(this))
    , m_hint(new DLineEdit(this))
    , m_submit(new InhibitButton(this))
    , m_cancel(new InhibitButton(this))
    , m_blurEffectWidget(new DBlurEffectWidget(this))
    , m_strength(new SecurityLevelItem(this))
    , m_warning(new DLabel(this))
{
    m_localSecurityCheck = DConfigHelper::instance()->getConfig(QStringLiteral("localPasswordSecurityCheck"), true).toBool();
    DConfigHelper::instance()->bind(this, QStringLiteral("localPasswordSecurityCheck"),
        [](const QString &, const QVariant &value, QObject *object) {
            auto *page = qobject_cast<LockPasswordChangeWidget *>(object);
            page->m_localSecurityCheck = value.toBool();
            if (!page->m_busy) {
                page->clearAlerts();
            }
        });
    setAccessibleName(QStringLiteral("LockPasswordChangeWidget"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *inputLayout = new QVBoxLayout;
    inputLayout->setContentsMargins(10, 0, 10, 0);
    inputLayout->setSpacing(6);
    m_avatar->setAvatarSize(UserAvatar::AvatarLargeSize);
    m_avatar->setObjectName("lockPasswordChangeAvatar");
    m_avatar->setFocusPolicy(Qt::NoFocus);
    inputLayout->addWidget(m_avatar, 0, Qt::AlignHCenter);
    inputLayout->addWidget(m_userName, 0, Qt::AlignHCenter);
    auto warningPixmap = QIcon(QStringLiteral(":/misc/images/dialog_warning.svg")).pixmap(QSize(16, 16) * devicePixelRatioF());
    warningPixmap.setDevicePixelRatio(devicePixelRatioF());
    m_warning->setPixmap(warningPixmap);
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::PlainText);
    auto *tipLayout = new QHBoxLayout;
    tipLayout->setSpacing(10);
    tipLayout->addStretch();
    tipLayout->addWidget(m_warning);
    tipLayout->addWidget(m_info);
    tipLayout->addStretch();
    inputLayout->addLayout(tipLayout);
    // Backend policy errors can be long. Keep them plain and bounded while
    // field validation uses the same alert presentation as eagle.
    m_error->setReadOnly(true);
    m_error->setFrameShape(QFrame::NoFrame);
    m_error->setStyleSheet(QStringLiteral("background: transparent;"));
    m_error->setMinimumHeight(32);
    m_error->setMaximumHeight(64);
    m_error->setObjectName("lockPasswordChangeError");
    inputLayout->addWidget(m_error);
    inputLayout->addSpacing(10);
    inputLayout->addWidget(m_current);
    auto *strengthLayout = new QHBoxLayout;
    strengthLayout->addStretch();
    m_strength->setFixedHeight(16);
    strengthLayout->addWidget(m_strength);
    inputLayout->addLayout(strengthLayout);
    inputLayout->addWidget(m_password);
    inputLayout->addSpacing(10);
    inputLayout->addWidget(m_confirmation);
    inputLayout->addSpacing(10);
    inputLayout->addWidget(m_hint);
    m_current->setPlaceholderText(tr("Please enter the old password."));
    m_password->setPlaceholderText(tr("Please enter the new password."));
    m_confirmation->setPlaceholderText(tr("Confirm the new password."));
    m_current->setAccessibleName(QStringLiteral("oldpasswordedit"));
    m_password->setAccessibleName(QStringLiteral("newpasswordedit"));
    m_confirmation->setAccessibleName(QStringLiteral("repeatpasswordedit"));
    m_hint->setAccessibleName(QStringLiteral("passwordtipsedit"));
    m_hint->setPlaceholderText(tr("Password Hint(optional)"));
    m_hint->setClearButtonEnabled(false);
    m_hint->lineEdit()->setAlignment(Qt::AlignCenter);
    m_hint->lineEdit()->setMaxLength(14);
    for (auto *edit : {m_current, m_password, m_confirmation}) {
        edit->lineEdit()->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText);
        connect(edit, &DLineEdit::returnPressed, this, &LockPasswordChangeWidget::submit);
        connect(edit, &DLineEdit::textEdited, this, [this, edit] {
            edit->setAlert(false);
            edit->hideAlertMessage();
            m_error->clear();
            m_error->hide();
            emit userIsActive();
        });
    }
    connect(m_password, &DLineEdit::textChanged, this, [this](const QString &password) {
        const auto level = password.isEmpty() ? PASSWORD_STRENGTH_LEVEL_ERROR
                                             : get_new_passwd_strength_level(password.toUtf8().constData());
        m_strength->setLevel(level == PASSWORD_STRENGTH_LEVEL_HIGH ? SecurityLevelItem::HighLevel
                             : level == PASSWORD_STRENGTH_LEVEL_MIDDLE ? SecurityLevelItem::MidLevel
                             : level == PASSWORD_STRENGTH_LEVEL_LOW ? SecurityLevelItem::LowLevel
                             : SecurityLevelItem::NoneLevel);
    });
    connect(m_password, &DLineEdit::textEdited, this, [this](const QString &password) {
        const auto error = password.isEmpty() ? QString() : localPasswordError(password);
        m_password->setAlert(!error.isEmpty());
        if (!error.isEmpty()) {
            m_password->showAlertMessage(error, m_password, 2000);
        }
        if (m_confirmation->text() == password) {
            m_confirmation->setAlert(false);
            m_confirmation->hideAlertMessage();
        }
    });
    connect(m_hint, &DLineEdit::textEdited, this, [this] {
        m_hint->setAlert(false);
        m_hint->hideAlertMessage();
        emit userIsActive();
    });
    m_cancel->setText(tr("Cancel"));
    m_submit->setText(tr("Save"));
    for (auto *button : {m_cancel, m_submit}) {
        button->resetHeigth(40);
        button->resetRound(8);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setFocusBorderColor(palette().color(QPalette::Highlight));
    }
    const auto setIcons = [this](InhibitButton *button, const QString &name) {
        auto normal = QIcon(QStringLiteral(":/misc/images/password_%1.svg").arg(name)).pixmap(QSize(16, 16) * devicePixelRatioF());
        auto hover = QIcon(QStringLiteral(":/misc/images/password_%1_hover.svg").arg(name)).pixmap(QSize(16, 16) * devicePixelRatioF());
        normal.setDevicePixelRatio(devicePixelRatioF());
        hover.setDevicePixelRatio(devicePixelRatioF());
        button->setNormalPixmap(normal);
        button->setHoverPixmap(hover);
    };
    setIcons(m_cancel, QStringLiteral("cancel"));
    setIcons(m_submit, QStringLiteral("submit"));
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_cancel, 90);
    buttons->addStretch(30);
    buttons->addWidget(m_submit, 90);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addLayout(inputLayout);
    layout->addSpacing(40);
    layout->addLayout(buttons);
    connect(m_submit, &InhibitButton::clicked, this, &LockPasswordChangeWidget::submit);
    connect(m_hint, &DLineEdit::returnPressed, this, &LockPasswordChangeWidget::submit);
    connect(m_cancel, &InhibitButton::clicked, this, [this] {
        if (!m_busy) {
            emit requestCancel();
        }
    });
    setTabOrder(m_current, m_password);
    setTabOrder(m_password, m_confirmation);
    setTabOrder(m_confirmation, m_hint);
    setTabOrder(m_hint, m_cancel);
    setTabOrder(m_cancel, m_submit);
    m_blurEffectWidget->setMaskColor(DBlurEffectWidget::LightColor);
    m_blurEffectWidget->setMaskAlpha(70);
    m_blurEffectWidget->setBlurRectXRadius(15);
    m_blurEffectWidget->setBlurRectYRadius(15);
    m_blurEffectWidget->lower();
    m_blurEffectWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    for (auto *widget : findChildren<QWidget *>()) {
        widget->installEventFilter(this);
    }
    setState(false, false, false, {});
}

void LockPasswordChangeWidget::setUser(const std::shared_ptr<User> &user)
{
    if (!user) {
        return;
    }
    m_accountName = user->name();
    m_fullName = user->fullName();
    m_avatar->setIcon(user->avatar());
    m_userName->updateUserName(user->name());
    m_userName->updateFullName(user->displayName());
    m_userName->setDomainUserVisible(user->isDomainUser());
}

void LockPasswordChangeWidget::setState(bool currentRequired, bool checkingOnly, bool busy, const QString &error)
{
    const bool refocus = m_currentRequired != currentRequired || m_checkingOnly != checkingOnly || m_busy != busy;
    if (busy && !m_busy) {
        clearInputs();
    }
    m_currentRequired = currentRequired;
    m_checkingOnly = checkingOnly;
    m_busy = busy;
    m_current->setVisible(currentRequired);
    m_password->setVisible(!checkingOnly);
    m_confirmation->setVisible(!checkingOnly);
    m_hint->setVisible(!checkingOnly);
    m_strength->setVisible(!checkingOnly);
    m_warning->setVisible(!checkingOnly);
    for (auto *edit : QList<DLineEdit *> {m_current, m_password, m_confirmation, m_hint}) {
        edit->setEnabled(!busy);
    }
    m_submit->setEnabled(!busy);
    m_cancel->setEnabled(!busy);
    m_submit->setText(checkingOnly ? tr("Unlock") : tr("Save"));
    m_info->setText(busy ? (checkingOnly ? tr("Checking password expiration...") : tr("Changing password..."))
                        : (checkingOnly ? tr("Please enter the old password.") : tr("Password expired. Please change it.")));
    m_error->setPlainText(error);
    m_error->setVisible(!error.isEmpty());
    if (!error.isEmpty() && !busy) {
        auto *input = currentRequired ? m_current : m_password;
        input->setAlert(true);
        input->selectAll();
    }
    updateGeometry();
    updateBlurEffectGeometry();
    if (refocus && !busy && isVisible()) {
        focusInput();
    }
}

QString LockPasswordChangeWidget::localPasswordError(const QString &password) const
{
    if (!m_localSecurityCheck) {
        return {};
    }
    for (const auto &account : {m_fullName, m_accountName}) {
        if (account.isEmpty()) {
            continue;
        }
        const auto error = deepin_pw_check(account.toUtf8().constData(), password.toUtf8().constData(), LEVEL_STRICT_CHECK, nullptr);
        // Match eagle: history/reuse is checked by passwd/PAM with the account.
        if (error == PW_NO_ERR || error == PW_ERR_PW_REPEAT) {
            continue;
        }
        switch (error) {
        case PW_ERR_LENGTH_SHORT: return tr("Password must have at least %1 characters").arg(get_pw_min_length(LEVEL_STRICT_CHECK));
        case PW_ERR_LENGTH_LONG: return tr("Password must be no more than %1 characters").arg(get_pw_max_length(LEVEL_STRICT_CHECK));
        case PW_ERR_CHARACTER_INVALID: return tr("Use English letters, numbers, half-width symbols and spaces for the new password.");
        case PW_ERR_SAME_AS_USERNAME: return tr("The password should differ from the username");
        case PW_ERR_CHARACTER_TYPE_TOO_FEW: return tr("Password must contain at least %1 types of characters").arg(get_pw_min_character_type(LEVEL_STRICT_CHECK));
        default: return tr("Password does not meet the local password rules.");
        }
    }
    return {};
}

void LockPasswordChangeWidget::clearAlerts()
{
    for (auto *edit : QList<DLineEdit *> {m_current, m_password, m_confirmation, m_hint}) {
        edit->setAlert(false);
        edit->hideAlertMessage();
    }
    m_error->clear();
    m_error->hide();
}

void LockPasswordChangeWidget::showFieldError(DLineEdit *edit, const QString &error)
{
    edit->setAlert(true);
    edit->showAlertMessage(error, edit, 2000);
    edit->lineEdit()->setFocus();
    edit->lineEdit()->selectAll();
}

bool LockPasswordChangeWidget::validateInputs()
{
    clearAlerts();
    if (m_currentRequired && m_current->text().isEmpty()) {
        showFieldError(m_current, tr("Password cannot be empty"));
        return false;
    }
    if (m_checkingOnly) {
        return true;
    }
    if (m_password->text().isEmpty() || m_confirmation->text().isEmpty()) {
        showFieldError(m_password->text().isEmpty() ? m_password : m_confirmation, tr("Password cannot be empty"));
        return false;
    }
    if (m_password->text() != m_confirmation->text()) {
        showFieldError(m_confirmation, tr("The two password inputs are inconsistent."));
        return false;
    }
    if (m_currentRequired && m_current->text() == m_password->text()) {
        showFieldError(m_password, tr("New password must differ from the current password"));
        return false;
    }
    const auto error = localPasswordError(m_password->text());
    if (!error.isEmpty()) {
        showFieldError(m_password, error);
        return false;
    }
    if (m_hint->text().contains(m_password->text())
        || (!m_current->text().isEmpty() && m_hint->text().contains(m_current->text()))) {
        showFieldError(m_hint, tr("The hint is visible to all users. Do not include the password here."));
        return false;
    }
    return true;
}

void LockPasswordChangeWidget::submit()
{
    if (!m_busy && validateInputs()) {
        emit userIsActive();
        emit requestChangePassword(m_current->text(), m_password->text(), m_confirmation->text(), m_hint->text());
    }
}

void LockPasswordChangeWidget::clearInputs()
{
    for (auto *edit : {m_current, m_password, m_confirmation}) {
        edit->clear();
    }
    // Discard undo/redo history as well, including rejected draft hints.
    m_hint->lineEdit()->setText(QString());
    clearAlerts();
}

void LockPasswordChangeWidget::focusInput()
{
    auto *input = m_currentRequired ? m_current : m_password;
    setFocusProxy(input->lineEdit());
    input->lineEdit()->setFocus();
}

void LockPasswordChangeWidget::updateBlurEffectGeometry()
{
    const auto bottom = m_checkingOnly ? m_current->geometry().bottom() : m_hint->geometry().bottom();
    m_blurEffectWidget->setGeometry(QRect(0, m_avatar->geometry().top() + m_avatar->height() / 2,
                                        width(), qMax(0, bottom + 14 - m_avatar->height() / 2)));
}

void LockPasswordChangeWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateBlurEffectGeometry();
}

void LockPasswordChangeWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateBlurEffectGeometry();
    if (!m_busy) {
        focusInput();
    }
}

void LockPasswordChangeWidget::hideEvent(QHideEvent *event)
{
    // Moving the shared content between screens hides/reparents/shows it in
    // one turn. Only a sustained hide is an exit from the visible form.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible())
            clearInputs();
    });
    QWidget::hideEvent(event);
}

bool LockPasswordChangeWidget::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            emit requestCancel();
            return true;
        }
        Q_FALLTHROUGH();
    case QEvent::KeyRelease:
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        emit userIsActive();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(object, event);
}
