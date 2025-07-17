// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userswiththesamename.h"

#include "sessionbasemodel.h"
#include "userpanel.h"

#include <DFontSizeManager>
#include <DHiDPIHelper>

#include <QVBoxLayout>
#include <QKeyEvent>

DWIDGET_USE_NAMESPACE

const int UserPanelSpacing = 20;

UsersWithTheSameName::UsersWithTheSameName(SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_backButton(new InhibitButton(this))
{
    setObjectName(QStringLiteral("UsersWithTheSameName"));
    setAccessibleName(QStringLiteral("UsersWithTheSameName"));

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);

    initUI();
}

void UsersWithTheSameName::initUI()
{
    m_backButton->setText(tr("Return"));
    const QPixmap &normalBackPixmap = DHiDPIHelper::loadNxPixmap(":/img/back-normal.svg");
    m_backButton->setNormalPixmap(normalBackPixmap);
    const QPixmap &hoverBackPixmap = DHiDPIHelper::loadNxPixmap(":/img/back-hover.svg");
    m_backButton->setHoverPixmap(hoverBackPixmap);
    m_backButton->setFixedSize(200, 64);
    DFontSizeManager::instance()->bind(m_backButton, DFontSizeManager::T4);

    QLabel *tipsLabel = new QLabel(this);
    tipsLabel->setWordWrap(false);
    tipsLabel->setAlignment(Qt::AlignCenter);
    tipsLabel->setText(tr("Please select the account for login"));
    DFontSizeManager::instance()->bind(tipsLabel, DFontSizeManager::T5);

    m_centerWidget = new QWidget;
    m_centerWidget->setAccessibleName("UsersWithTheSameNameCenterWidget");

    m_panelLayout = new QVBoxLayout(m_centerWidget);
    m_panelLayout->setContentsMargins(10, 10, 10, 10);
    m_panelLayout->setSpacing(UserPanelSpacing);

    m_centerWidget->setAutoFillBackground(false);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addStretch();
    mainLayout->addWidget(tipsLabel, 0, Qt::AlignCenter);
    mainLayout->addSpacing(40);
    mainLayout->addWidget(m_centerWidget, 0, Qt::AlignCenter);
    mainLayout->addSpacing(90);
    mainLayout->addWidget(m_backButton, 0, Qt::AlignCenter);
    mainLayout->addStretch();

    connect(m_backButton, &InhibitButton::clicked, this, &UsersWithTheSameName::clicked);
    connect(m_backButton, &InhibitButton::mouseEnter, this, [this] {
        updateCurrentSelectedUser(true);
    });
    connect(m_backButton, &InhibitButton::mouseLeave, this, [this] {
        updateCurrentSelectedUser(false);
    });
}

void UsersWithTheSameName::clearUserPanelList()
{
    // 释放之前创建的用户列表窗体
    for (auto widget : m_loginWidgets) {
        widget->deleteLater();
    }
    m_loginWidgets.clear();
}

bool UsersWithTheSameName::findUsers(const QString nativeUserName, const QString &doMainAccountDetail)
{
    auto createUserPanel = [this](const QString &avatar, const QString &fullName, const QString &name, const QString &type, const uint uid)-> UserPanel * {
        UserPanel *widget = new UserPanel(m_centerWidget);
        // 初始化用户信息
        widget->setAvatar(avatar);
        widget->setName(name);
        widget->setFullName(fullName);
        widget->setDisplayName(fullName.isEmpty() ? name : fullName);
        widget->setType(type);
        widget->setUid(uid);
        connect(widget, &UserPanel::requestAction, this, &UsersWithTheSameName::onUserClicked);
        connect(widget, &UserPanel::mouseEnter, this, [this] {
            updateCurrentSelectedUser(true);
        });
        connect(widget, &UserPanel::mouseLeave, this, [this] {
            updateCurrentSelectedUser(false);
        });
        return widget;
    };
    clearUserPanelList();
    std::shared_ptr<User> nativeUser = m_model->findUserByName(nativeUserName);
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(doMainAccountDetail.toUtf8());
    if (!jsonDoc.isObject()) {
        qCWarning(DDE_SHELL) << "Invalid JSON data.";
        if (nativeUser) {
            Q_EMIT requestCheckAccount(nativeUser->name());
        }
        clearUserPanelList();
        return false;
    }
    const QJsonObject &jsonObject = jsonDoc.object();
    QString fullName;
    // 检查fullname字段是否存在，如果存在则使用，否则使用name字段的值
    if (jsonObject.contains("fullname")) {
        fullName = jsonObject.value("fullname").toString();
    } else {
        fullName = jsonObject.value("name").toString();
    }
    const QString &name = jsonObject.value("name").toString();
    const uint uid = jsonObject.value("uid").toInt();

    if (name == nativeUserName) {
        qCWarning(DDE_SHELL) << "Domain user name is the same as the native user name";
        if (nativeUser) {
            Q_EMIT requestCheckAccount(nativeUser->name());
        }
        clearUserPanelList();
        return false;
    }

    if (nativeUser) {
        UserPanel *widget = createUserPanel(nativeUser->avatar(), nativeUser->fullName(), nativeUser->name(), tr("Local Account"), nativeUser->uid());
        m_loginWidgets.push_back(widget);
    }

    //  检查本地是否已经创建过域账户
    std::shared_ptr<User> domainUser = m_model->findUserByName(name);
    UserPanel *widget = domainUser ? createUserPanel(domainUser->avatar(), domainUser->fullName(), domainUser->name(), tr("Domain Account"), domainUser->uid())
                                  : createUserPanel(":/img/default_avatar.svg", fullName, name, tr("Domain Account"), uid);
    m_loginWidgets.push_back(widget);

    addUserPanel(m_loginWidgets);
    return true;
}

//创建用户窗体
void UsersWithTheSameName::addUserPanel(QList<UserPanel *> &list)
{
    //多用户的情况按照其uid排序，升序排列，符合账户先后创建顺序
    std::sort(list.begin(), list.end(), [](UserPanel *w1, UserPanel *w2) {
        return (w1->uid() < w2->uid());
    });
    for (int index = 0; index < list.size(); ++index) {
        m_panelLayout->insertWidget(index, list[index]);
    }

    m_centerWidget->setFixedSize(UserPanelWidth + 20, list.size() * (UserPanelHeight + UserPanelSpacing) - UserPanelSpacing);
}

//点击用户
void UsersWithTheSameName::onUserClicked()
{
    UserPanel *widget = static_cast<UserPanel *>(sender());
    if (!widget) return;

    m_currentSelectedUser = widget;
    Q_EMIT clicked();
    Q_EMIT requestCheckAccount(widget->name());
}

/**
 * @brief 切换下一个用户
 */
void UsersWithTheSameName::switchNextUser()
{
    for (int i = 0; i < m_loginWidgets.size(); ++i) {
        if (m_loginWidgets[i]->isSelected()) {
            m_loginWidgets[i]->setSelected(false);
            if (i + 1 < (m_loginWidgets.size())) {
                m_currentSelectedUser = m_loginWidgets[i + 1];
                m_currentSelectedUser->setSelected(true);

            } else {
                m_currentSelectedUser = nullptr;
                m_backButton->setSelected(true);
            }
            break;
        } else if (i == (m_loginWidgets.size() - 1)) {
            m_currentSelectedUser = m_loginWidgets.first();
            m_currentSelectedUser->setSelected(true);
            m_backButton->setSelected(false);
            break;
        }
    }
}

/**
 * @brief 切换上一个用户
 */
void UsersWithTheSameName::switchPreviousUser()
{
    for (int i = 0; i < m_loginWidgets.size(); ++i) {
        if (m_loginWidgets[i]->isSelected()) {
            m_loginWidgets[i]->setSelected(false);
            if (i - 1 >= 0) {
                m_currentSelectedUser = m_loginWidgets[i - 1];
                m_currentSelectedUser->setSelected(true);
                m_backButton->setSelected(false);
            } else {
                m_currentSelectedUser = nullptr;
                m_backButton->setSelected(true);
            }
            break;
        } else if (i == (m_loginWidgets.size() - 1)) {
            if (m_backButton->isSelected()) {
                m_currentSelectedUser = m_loginWidgets.last();
                m_currentSelectedUser->setSelected(true);
                m_backButton->setSelected(false);
                break;
            }
            m_currentSelectedUser = nullptr;
            m_backButton->setSelected(true);
            break;
        }
    }
}

void UsersWithTheSameName::hideEvent(QHideEvent *event)
{
    return QWidget::hideEvent(event);
}

void UsersWithTheSameName::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        switchPreviousUser();
        break;
    case Qt::Key_Down:
        switchNextUser();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_backButton->isSelected()) {
            Q_EMIT clicked();
            break;
        }
        for (auto it = m_loginWidgets.constBegin(); it != m_loginWidgets.constEnd(); ++it) {
            if ((*it)->isSelected()) {
                Q_EMIT(*it)->requestAction();
                break;
            }
        }
        break;
    case Qt::Key_Escape:
        Q_EMIT clicked();
        Q_EMIT requestCheckAccount(m_model->currentUser()->name());
        break;
    default:
        break;
    }
    return QWidget::keyPressEvent(event);
}

void UsersWithTheSameName::updateCurrentSelectedUser(bool isMouseEner)
{
    if (!isMouseEner || m_backButton->underMouse()) {
        if (m_currentSelectedUser != nullptr)
            m_currentSelectedUser->setSelected(false);
    } else {
        for (int i = 0; i < m_loginWidgets.size(); ++i) {
            if (m_loginWidgets[i]->underMouse() && !m_loginWidgets[i]->isSelected()) {
                if (m_currentSelectedUser)
                    m_currentSelectedUser->setSelected(false);
                m_currentSelectedUser = m_loginWidgets[i];
                m_currentSelectedUser->setSelected(true);
                m_backButton->setSelected(false);
                break;
            }
        }
    }

}

