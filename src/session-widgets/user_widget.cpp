// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "user_widget.h"
#include "user_name_widget.h"
#include "useravatar.h"
#include "dconfig_helper.h"
#include "constants.h"

#include <DFontSizeManager>
#include <DHiDPIHelper>

const int BlurRadius = 15;
const int BlurTransparency = 70;
const int UserDisplayNameHeight = 42;

using namespace DDESESSIONCC;

UserWidget::UserWidget(QWidget *parent)
    : QWidget(parent)
    , m_isSelected(false)
    , m_uid(UINT_MAX)
    , m_mainLayout(new QVBoxLayout(this))
    , m_blurEffectWidget(new DBlurEffectWidget(this))
    , m_avatar(new UserAvatar(this))
    , m_loginState(new DLabel(this))
    , m_displayNameLabel(new DLabel(this))
    , m_displayNameWidget(new QWidget(this))
    , m_userNameWidget(nullptr)
{
    setObjectName(QStringLiteral("UserWidget"));
    setAccessibleName(QStringLiteral("UserWidget"));

    setGeometry(0, 0, 280, 176);
    setFixedSize(UserFrameWidth, UserFrameHeight);

    setFocusPolicy(Qt::NoFocus);
}

void UserWidget::initUI()
{
    /* 头像 */
    m_avatar->setFocusPolicy(Qt::NoFocus);
    m_avatar->setIcon(m_user->avatar());
    m_avatar->setAvatarSize(UserAvatar::AvatarSmallSize);

    /* 用户全名 */
    m_displayNameWidget->setAccessibleName(QStringLiteral("NameWidget"));
    QHBoxLayout *nameLayout = new QHBoxLayout(m_displayNameWidget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    nameLayout->setContentsMargins(0, 0, 0, 0);
#else
    nameLayout->setMargin(0);
#endif

    nameLayout->addStretch();
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(":/misc/images/select.svg");
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_loginState->setAccessibleName("LoginState");
    m_loginState->setPixmap(pixmap);
    m_loginState->setVisible(m_user->isLogin());
    QVBoxLayout *loginStateLayout = new QVBoxLayout;
    loginStateLayout->setSpacing(0);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    loginStateLayout->setContentsMargins(0, 0, 0, 0);
#else
    loginStateLayout->setMargin(0);
#endif
    loginStateLayout->addSpacing(4);
    loginStateLayout->addWidget(m_loginState);
    nameLayout->addLayout(loginStateLayout);

    // 域账户标识
    QPixmap domainUserpixmap = QIcon::fromTheme(":/misc/images/domainUser.svg").pixmap(24, 24);
    domainUserpixmap.setDevicePixelRatio(devicePixelRatioF());
    DLabel *domainUserLabel = new DLabel(this);
    domainUserLabel->setAccessibleName("isDomainUser");
    domainUserLabel->setPixmap(domainUserpixmap);
    domainUserLabel->setVisible(m_user->isDomainUser());
    domainUserLabel->setAlignment(Qt::AlignVCenter);
    QVBoxLayout *isDomainUserLayout = new QVBoxLayout;
    isDomainUserLayout->setContentsMargins(0, 5, 0, 0);
    isDomainUserLayout->addWidget(domainUserLabel);
    nameLayout->addLayout(isDomainUserLayout);

    m_displayNameLabel->setAccessibleName("NameLabel");
    m_displayNameLabel->setTextFormat(Qt::TextFormat::PlainText);
    m_displayNameLabel->setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Fixed);
    m_displayNameLabel->setFixedHeight(UserDisplayNameHeight);
    m_displayNameLabel->setText(m_user->displayName());
    DFontSizeManager::instance()->bind(m_displayNameLabel, DFontSizeManager::T2);
    QPalette palette = m_displayNameLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_displayNameLabel->setPalette(palette);
    nameLayout->addWidget(m_displayNameLabel);
    nameLayout->addStretch();

    // 用户名，根据配置决定是否构造对象
    if (DConfigHelper::instance()->getConfig(SHOW_USER_NAME, false).toBool()) {
        m_userNameWidget = new UserNameWidget(true, false, this);
        m_userNameWidget->updateFullName(m_user->fullName());
        setFixedHeight(heightHint());
    }
    /* 模糊背景 */
    m_blurEffectWidget->setMaskColor(DBlurEffectWidget::LightColor);
    m_blurEffectWidget->setMaskAlpha(BlurTransparency);
    m_blurEffectWidget->setBlurRectXRadius(BlurRadius);
    m_blurEffectWidget->setBlurRectYRadius(BlurRadius);

    m_mainLayout->setSpacing(0);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
#else
    m_mainLayout->setMargin(0);
#endif
    m_mainLayout->addWidget(m_avatar);
    m_mainLayout->addSpacing(4);
    m_mainLayout->addWidget(m_displayNameWidget);
    if (m_userNameWidget)
        m_mainLayout->addWidget(m_userNameWidget);
    m_mainLayout->addSpacing(20);
    m_mainLayout->addStretch(1);

    DConfigHelper::instance()->bind(this, SHOW_USER_NAME, &UserWidget::onDConfigPropertyChanged);
    DConfigHelper::instance()->bind(this, USER_FRAME_MAX_WIDTH, &UserWidget::onDConfigPropertyChanged);
}

void UserWidget::initConnections()
{
    connect(m_user.get(), &User::avatarChanged, this, &UserWidget::setAvatar);
    connect(m_user.get(), &User::displayNameChanged, this, &UserWidget::updateUserNameLabel);
    connect(m_user.get(), &User::loginStateChanged, this, &UserWidget::setLoginState);
    connect(qGuiApp, &QGuiApplication::fontChanged, this, &UserWidget::updateUserNameLabel);
    connect(m_avatar, &UserAvatar::clicked, this, &UserWidget::clicked);
}

/**
 * @brief 设置用户信息
 * @param user
 */
void UserWidget::setUser(std::shared_ptr<User> user)
{
    m_user = user;

    initUI();
    initConnections();

    setUid(user->uid());
    updateUserNameLabel();
}

const std::shared_ptr<User> &UserWidget::user() const
{
    return m_user;
}

/**
 * @brief 设置用户选中标识
 * @param isSelected
 */
void UserWidget::setSelected(bool isSelected)
{
    m_isSelected = isSelected;
    update();
}

/**
 * @brief 设置用户选中标识
 * @param isSelected
 */
void UserWidget::setFastSelected(bool isSelected)
{
    m_isSelected = isSelected;
    repaint();
}

/**
 * @brief 给当前 Widget 设置一个 id，用于排序
 * @param uid
 */
void UserWidget::setUid(const uint uid)
{
    m_uid = uid;
}

/**
 * @brief 设置用户头像
 * @param avatar
 */
void UserWidget::setAvatar(const QString &avatar)
{
    m_avatar->setIcon(avatar);
}

/**
 * @brief 设置用户名字体
 * @param font
 */
void UserWidget::updateUserNameLabel()
{
    const bool showUserName = DConfigHelper::instance()->getConfig(SHOW_USER_NAME, false).toBool();
    const QString &name = showUserName ? m_user->name() : m_user->displayName();
    int nameWidth = m_displayNameLabel->fontMetrics().boundingRect(name).width();

    //切换账号时，是否显示全部账号
    bool ok;
    int userFrameMaxWidth = DConfigHelper::instance()->getConfig(USER_FRAME_MAX_WIDTH, UserFrameWidth).toInt(&ok);
    if (!ok) userFrameMaxWidth = UserFrameWidth;
    int maxFontWidth = nameWidth + 25 * 2;
    int labelMaxWidth = userFrameMaxWidth - 25 * 2;
    setFixedWidth(maxFontWidth >= userFrameMaxWidth ? userFrameMaxWidth : maxFontWidth >= UserFrameWidth ? maxFontWidth : UserFrameWidth);
    if (maxFontWidth > userFrameMaxWidth) {
        QString str = m_displayNameLabel->fontMetrics().elidedText(name, Qt::ElideRight, labelMaxWidth);
        m_displayNameLabel->setText(str);
    } else {
        m_displayNameLabel->setText(name);
    }

    if (m_userNameWidget)
        m_userNameWidget->updateFullName(m_user->fullName());
}

/**
 * @brief 设置登录状态
 * @param isLogin
 */
void UserWidget::setLoginState(const bool isLogin)
{
    m_loginState->setVisible(isLogin);
}

/**
 * @brief 更新模糊背景参数
 */
void UserWidget::updateBlurEffectGeometry()
{
    QRect rect = layout()->geometry();
    rect.setTop(m_avatar->geometry().top() + m_avatar->height() / 2);
    rect.setBottom(rect.bottom() - 18);
    m_blurEffectWidget->setGeometry(rect);
}

void UserWidget::mousePressEvent(QMouseEvent *event)
{
    emit clicked();
    QWidget::mousePressEvent(event);
}

void UserWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 不再向上传递mouseReleaseEvent信号，避免影响用户列表模式处理点击空白处的逻辑
    Q_UNUSED(event)
}

void UserWidget::paintEvent(QPaintEvent *event)
{
    if (m_isSelected) {
        QPainter painter(this);
        painter.setPen(QColor(255, 255, 255, 76));
        painter.setBrush(QColor(255, 255, 255, 76));
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawRoundedRect(QRect(width() / 2 - 46, rect().bottom() - 4, 92, 4), 2, 2);
    }
    QWidget::paintEvent(event);
}

void UserWidget::resizeEvent(QResizeEvent *event)
{
    updateBlurEffectGeometry();
    QWidget::resizeEvent(event);
}

int UserWidget::heightHint() const
{
    if (m_userNameWidget)
        return UserFrameHeight + m_userNameWidget->heightHint();

    return UserFrameHeight;
}

void UserWidget::onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<UserWidget*>(objPtr);
    if (!obj)
        return;

    qCInfo(DDE_SHELL) << "DConfig property changed, key: " << key << ", value: " << value;
    if (key == SHOW_USER_NAME) {
        const bool showUserName = value.toBool();
        if (showUserName) {
            if (!obj->m_userNameWidget) {
                obj->m_userNameWidget = new UserNameWidget(true, false, obj);
            }
            obj->m_mainLayout->insertWidget(obj->m_mainLayout->indexOf(obj->m_displayNameWidget) + 1, obj->m_userNameWidget);
            obj->m_userNameWidget->show();
            if (obj->m_user)
                obj->m_userNameWidget->updateFullName(obj->m_user->fullName());

        } else {
            if (obj->m_userNameWidget) {
                delete obj->m_userNameWidget;
                obj->m_userNameWidget = nullptr;
            }
        }
    }

    // 刷新界面
    obj->updateUserNameLabel();
    obj->setFixedHeight(obj->heightHint());
    obj->updateBlurEffectGeometry();
    obj->update();
}
