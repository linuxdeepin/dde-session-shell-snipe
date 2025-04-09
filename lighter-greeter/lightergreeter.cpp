// SPDX-FileCopyrightText: 2015 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QListView>
#include <QComboBox>
#include <QDebug>
#include <QProcess>
#include <QFile>

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QLightDM/UsersModel>

#include <DBlurEffectWidget>
#include <DConfig>

#include "lightergreeter.h"
#include "timewidget.h"
#include "dlineeditex.h"
#include "transparentbutton.h"
#include "useravatar.h"
#include "public_func.h"
#include "dconfig_helper.h"

#define AVATAR_ICON_SIZE UserAvatar::AvatarSmallSize
#define AVATAR_SIZE UserAvatar::AvatarLargeSize
#define SPACING 10

LighterGreeter::LighterGreeter(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_loginFrame(new DBlurEffectWidget(this))
    , m_avatar(new UserAvatar(this))
    , m_userCbx(new QComboBox(this))
    , m_passwordEdit(new DLineEditEx(this))
    , m_sessionCbx(new QComboBox(this))
    , m_loginBtn(new TransparentButton(this))
    , m_switchGreeter(new QPushButton(tr("Standard Mode"), this))
    , m_greeter(new QLightDM::Greeter(this))
    , m_sessionModel(new QLightDM::SessionsModel(this))
    , m_allowSwitchToWayland(DConfigHelper::instance()->getConfig("allowSwitchingToWayland", false).toBool())
    , m_defaultSessionName(DConfigHelper::instance()->getConfig("defaultSession", "deepin").toString())
{
    if (!m_greeter->connectSync()) {
        qCritical() << "Connect sync failed";
        close();
    }

    initUI();
    initConnections();
}

void LighterGreeter::initUI()
{
    // 顶部区域
    m_mainLayout->addWidget(new TimeWidget(this), 1, Qt::AlignCenter);

    // 中间区域
    QVBoxLayout *layout = new QVBoxLayout;
    QVBoxLayout *vLayout = new QVBoxLayout(m_loginFrame);
    vLayout->setContentsMargins(SPACING, SPACING, SPACING, SPACING);
    vLayout->addSpacing(SPACING + (AVATAR_SIZE / 2));
    vLayout->addWidget(m_userCbx, 0, Qt::AlignCenter);
    vLayout->addWidget(m_passwordEdit, 0, Qt::AlignCenter);

    m_loginFrame->setMaskColor(DBlurEffectWidget::LightColor);
    m_loginFrame->setMaskAlpha(70);
    m_loginFrame->setBlurRectXRadius(15);
    m_loginFrame->setBlurRectYRadius(15);
    m_loginFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_loginFrame->setMaximumSize(1152, 300);
    m_userCbx->setMinimumWidth(300);
    m_passwordEdit->setMinimumWidth(300);
    m_passwordEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_passwordEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_passwordEdit->setClearButtonEnabled(false);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFocusPolicy(Qt::StrongFocus);
    m_passwordEdit->lineEdit()->setValidator(new QRegExpValidator(QRegExp("^[ -~]+$")));

    m_loginBtn->setIcon(DStyle::SP_LockElement);

    auto lockPalette = m_loginBtn->palette();
    QColor color = lockPalette.color(QPalette::Active, QPalette::Highlight);
    lockPalette.setColor(QPalette::Disabled, QPalette::Highlight, color);
    m_loginBtn->setPalette(lockPalette);

    // 头像始终在LoginFrame区域的上部中间位置
    m_avatar->setFocusPolicy(Qt::NoFocus);
    m_avatar->setIcon("/var/lib/AccountsService/icons/guest.png");
    m_avatar->setAvatarSize(AVATAR_ICON_SIZE);

    layout->addStretch();
    layout->addWidget(m_loginFrame, 0, Qt::AlignCenter);
    layout->addWidget(m_loginBtn, 0, Qt::AlignCenter);
    layout->addSpacing(10);
    layout->addStretch();

    m_mainLayout->addLayout(layout, 5);

    /* 整体布局，横向1:5:1,纵向1:1:1
    |---------------------------|
    |    |                  |   |
    |    |       Time       |   |
    |---------------------------|
    |    |                  |   |
    |    |      Login       |   |
    |---------------------------|
    |    |                  |   |
    |    |                  | s |
    |---------------------------|
    */

    // 右下角区域
    DBlurEffectWidget *controlFrame = new DBlurEffectWidget(this);
    QHBoxLayout *hLayout = new QHBoxLayout(controlFrame);
    hLayout->setContentsMargins(SPACING, SPACING, SPACING, SPACING);
    hLayout->addWidget(m_sessionCbx, 0, Qt::AlignCenter);
    hLayout->addWidget(m_switchGreeter, 0, Qt::AlignCenter);

    controlFrame->setMaskColor(DBlurEffectWidget::LightColor);
    controlFrame->setMaskAlpha(70);
    controlFrame->setBlurRectXRadius(15);
    controlFrame->setBlurRectYRadius(15);

    controlFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_mainLayout->addWidget(controlFrame, 1, Qt::AlignRight);

    m_userCbx->setModel(new QLightDM::UsersModel(this));

    for (int i = 0; i < m_sessionModel->rowCount(QModelIndex()); ++i) {
        const QString &sessionName = m_sessionModel->data(m_sessionModel->index(i), QLightDM::SessionsModel::IdRole).toString();
        QString displayName = (sessionName == "deepin") ? "X11" : sessionName;
        m_sessionCbx->addItem(displayName, sessionName);
    }

    if (!m_allowSwitchToWayland) {
        m_sessionCbx->removeItem(m_sessionCbx->findData("Wayland"));
    }

    if (!m_defaultSessionName.isEmpty()) {
        m_sessionCbx->setCurrentIndex(m_sessionCbx->findData(m_defaultSessionName));
    }

    m_sessionCbx->setVisible(m_sessionCbx->count() > 1);

    m_loginFrame->installEventFilter(this);
    installEventFilter(this);

    // Select last user
    const QString &user = DConfigHelper::instance()->getConfig("lastUser", QString()).toString();
    int index = m_userCbx->findText(user);
    if (index != -1)
        m_userCbx->setCurrentIndex(index);

    QMetaObject::invokeMethod(this, &LighterGreeter::updateFocus, Qt::QueuedConnection);
}

void LighterGreeter::initConnections()
{
    connect(m_passwordEdit, &DLineEditEx::returnPressed, this, &LighterGreeter::onStartAuthentication);
    connect(m_passwordEdit, &DLineEditEx::textChanged, this, [ = ] {
        m_passwordEdit->setAlert(false);
        m_passwordEdit->hideAlertMessage();
    });
    connect(m_userCbx, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LighterGreeter::updateFocus);
    connect(m_loginBtn, &TransparentButton::clicked, this, &LighterGreeter::onStartAuthentication);
    connect(m_switchGreeter, &QPushButton::clicked, this, &LighterGreeter::resetToNormalGreeter);

    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &LighterGreeter::onAuthenticationComplete);
    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &LighterGreeter::onShowPrompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &LighterGreeter::onShowMessage);
}

void LighterGreeter::respond()
{
    // Respond to prompt with password text and disable window
    m_greeter->respond(m_passwordEdit->text());

    m_userCbx->setEnabled(false);
    m_passwordEdit->setEnabled(false);
    m_loginBtn->setEnabled(false);
    m_sessionCbx->setEnabled(false);
    m_switchGreeter->setEnabled(false);

    // Start password field animation
    m_passwordEdit->startAnimation();
}

void LighterGreeter::onStartAuthentication()
{
    // Cancel ongoing authentication
    if (m_greeter->inAuthentication()) {
        m_greeter->cancelAuthentication();
    }

    // Begin new authentication process with selected user
    int index = m_userCbx->currentIndex();
    const QString &userName = m_userCbx->itemData(index, QLightDM::UsersModel::NameRole).toString();
    qInfo() << "Start authentication, user: " << userName;
    m_greeter->authenticate(userName);

    // Set focus to password field
    m_passwordEdit->setFocus();
}

void LighterGreeter::onAuthenticationComplete()
{
    qInfo() << "Authentication complete";
    if (m_greeter->isAuthenticated()) {
        auto startLogin = [this](const QString & session) {
            // Update cache with user information
            DConfigHelper::instance()->setConfig("lastUser", m_userCbx->currentText());

            // Hide current window and emit signal to start new session
            this->setVisible(false);
            Q_EMIT authFinished();

            // Set cursor to system default
            setRootWindowCursor();

            // Start session synchronization
            QMetaObject::invokeMethod(m_greeter, "startSessionSync", Qt::QueuedConnection
                                      , Q_ARG(QString, session));
        };

        startLogin(m_sessionCbx->currentData().toString());
    } else {
        qInfo() << "Authentication failed";
    }
}

void LighterGreeter::onShowPrompt(QString promptText, int promptType)
{
    qInfo() << "Showing prompt, text: " <<  promptText << ", type: " << promptType;

    // Stop password field animation and enable window
    m_passwordEdit->stopAnimation();

    m_userCbx->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_loginBtn->setEnabled(true);
    m_sessionCbx->setEnabled(true);
    m_switchGreeter->setEnabled(true);

    // Determine type of prompt being displayed
    auto type = static_cast<QLightDM::Greeter::PromptType>(promptType);
    switch (type) {
    case QLightDM::Greeter::PromptType::PromptTypeSecret:
    case QLightDM::Greeter::PromptType::PromptTypeQuestion:
        respond();

        // Not need show just respond password
        //        m_passwordEdit->clear();
        //        m_passwordEdit->setFocus();
        //        m_passwordEdit->setPlaceholderText(promptText);
        break;
    }
}

void LighterGreeter::onShowMessage(QString messageText, int messageType)
{
    qInfo() << "Showing message, text: " <<  messageText << ", type: " << messageType;

    // Stop password field animation and enable window
    m_passwordEdit->stopAnimation();

    m_userCbx->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_loginBtn->setEnabled(true);
    m_sessionCbx->setEnabled(true);
    m_switchGreeter->setEnabled(true);

    // Determine type of message being displayed
    auto type = static_cast<QLightDM::Greeter::MessageType>(messageType);
    switch (type) {
    case QLightDM::Greeter::MessageType::MessageTypeInfo:
        // Do nothing for info messages
        break;
    case QLightDM::Greeter::MessageType::MessageTypeError:
        // Clear password field, set alert flag, and show error message for error messages
        if (m_passwordEdit->isVisible()) {
            m_passwordEdit->clear();
            m_passwordEdit->setAlert(true);
            m_passwordEdit->showAlertMessage(messageText);
            m_passwordEdit->setFocus();
            m_passwordEdit->lineEdit()->setFocus();
        }
        break;
    }
}

void LighterGreeter::resetToNormalGreeter()
{
    // If it exits abnormally, try to pull up the normal Greeter, this part is handled in the script
    qApp->exit(-1);
}

void LighterGreeter::updateFocus()
{
    // Clear password field
    m_passwordEdit->clear();

    m_passwordEdit->setVisible(true);

    const QString &userName = m_userCbx->itemData(m_userCbx->currentIndex(), QLightDM::UsersModel::NameRole).toString();

    QFile group("/etc/group");
    if (group.open(QIODevice::ReadOnly)) {
        QByteArray content = group.readAll();
        QList<QByteArray> list = content.split('\n');
        for (auto item : list) {
            // Wether current user is in group nopasswordlogin
            if (item.startsWith("nopasswdlogin") && item.endsWith(userName.toStdString().data())) {
                m_passwordEdit->setVisible(false);
                m_loginBtn->setFocus();

                this->setTabOrder(m_loginBtn, m_sessionCbx);
                this->setTabOrder(m_sessionCbx, m_switchGreeter);
                this->setTabOrder(m_switchGreeter, m_userCbx);
                this->setTabOrder(m_userCbx, m_loginBtn);

                return;
            }
        }
    }

    m_passwordEdit->setFocus();
    m_passwordEdit->lineEdit()->setFocus();

    this->setTabOrder(m_loginBtn, m_sessionCbx);
    this->setTabOrder(m_sessionCbx, m_switchGreeter);
    this->setTabOrder(m_switchGreeter, m_userCbx);
    this->setTabOrder(m_userCbx, m_passwordEdit);
    this->setTabOrder(m_passwordEdit, m_loginBtn);
}

bool LighterGreeter::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this && event->type() == QEvent::Show) {
        // Set focus to password field when window is displayed
        if (!qgetenv("XDG_SESSION_TYPE").startsWith("wayland"))
            activateWindow();

        updateFocus();
    } else if ((watched == this || watched == m_loginFrame) && event->type() == QEvent::Resize) {
        int x = m_loginFrame->mapToGlobal(QPoint(0, 0)).x() + ((m_loginFrame->width() - AVATAR_SIZE) / 2);
        int y = m_loginFrame->mapToGlobal(QPoint(0, 0)).y() - (AVATAR_SIZE / 2);

        m_avatar->move(mapFromGlobal(QPoint(x, y)));
    }

    return QWidget::eventFilter(watched, event);
}
