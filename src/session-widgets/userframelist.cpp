// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userframelist.h"

#include "dconfig_helper.h"
#include "dflowlayout.h"
#include "framedatabind.h"
#include "sessionbasemodel.h"
#include "user_widget.h"
#include "userinfo.h"
#include "constants.h"

#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QVBoxLayout>

const int UserFrameSpaceing = 40;

using namespace DDESESSIONCC;

UserFrameList::UserFrameList(QWidget *parent)
    : QWidget(parent)
    , m_scrollArea(new QScrollArea(this))
    , m_frameDataBind(FrameDataBind::Instance())
{
    setObjectName(QStringLiteral("UserFrameList"));
    setAccessibleName(QStringLiteral("UserFrameList"));

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);

    initUI();

    // 设置用户列表支持触屏滑动,TouchGesture存在bug,滑动过程中会响应其他事件,打断滑动事件,改为LeftMouseButtonGesture
    QScroller::grabGesture(m_scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller *scroller = QScroller::scroller(m_scrollArea->viewport());
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootWhenScrollable);
    scroller->setScrollerProperties(sp);

    std::function<void(QVariant)> function = std::bind(&UserFrameList::onOtherPageChanged, this, std::placeholders::_1);
    int index = m_frameDataBind->registerFunction("UserFrameList", function);

    connect(this, &UserFrameList::destroyed, this, [this, index] {
        m_frameDataBind->unRegisterFunction("UserFrameList", index);
    });

    DConfigHelper::instance()->bind(this, SHOW_USER_NAME);
}

void UserFrameList::initUI()
{
    m_centerWidget = new QWidget;
    m_centerWidget->setAccessibleName("UserFrameListCenterWidget");

    m_flowLayout = new DFlowLayout(m_centerWidget);
    m_flowLayout->setFlow(QListView::LeftToRight);
    m_flowLayout->setContentsMargins(10, 10, 10, 10);
    m_flowLayout->setSpacing(40);

    m_scrollArea->setAccessibleName("UserFrameListCenterWidget");
    m_scrollArea->setWidget(m_centerWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setFocusPolicy(Qt::NoFocus);
    m_centerWidget->setAutoFillBackground(false);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_scrollArea, 0, Qt::AlignCenter);
}

//设置SessionBaseModel，创建用户列表窗体
void UserFrameList::setModel(SessionBaseModel *model)
{
    m_model = model;

    connect(model, &SessionBaseModel::userAdded, this, &UserFrameList::handlerBeforeAddUser);
    connect(model, &SessionBaseModel::userRemoved, this, &UserFrameList::removeUser);

    QList<std::shared_ptr<User>> userList = m_model->userList();
    for (auto user : userList) {
        handlerBeforeAddUser(user);
    }
}

void UserFrameList::setFixedSize(const QSize &size)
{
    QWidget::setFixedSize(size);
    //先确定界面大小，再根据界面大小计算用户列表区域大小
    updateLayout(width());
}

void UserFrameList::handlerBeforeAddUser(std::shared_ptr<User> user)
{
    if (m_model->isServerModel()) {
        if (user->isLogin() || user->type() == User::Default)
            addUser(user);
        connect(user.get(), &User::loginStateChanged, this, [=](bool is_login) {
            if (is_login) {
                addUser(user);
            } else {
                removeUser(user);
            }
        });
    } else {
        addUser(user);
    }
}

//创建用户窗体
void UserFrameList::addUser(const std::shared_ptr<User> user)
{
    UserWidget *widget = new UserWidget(m_centerWidget);
    widget->setUser(user);
    widget->setSelected(m_model->currentUser()->uid() == user->uid());

    connect(widget, &UserWidget::clicked, this, &UserFrameList::onUserClicked);

    //多用户的情况按照其uid排序，升序排列，符合账户先后创建顺序
    m_loginWidgets.push_back(widget);
    qSort(m_loginWidgets.begin(), m_loginWidgets.end(), [=](UserWidget *w1, UserWidget *w2) {
        return (w1->uid() < w2->uid());
    });
    int index = m_loginWidgets.indexOf(widget);
    m_flowLayout->insertWidget(index, widget);

    //添加用户和删除用户时，重新计算区域大小
    updateLayout(width());
}

//删除用户
void UserFrameList::removeUser(const std::shared_ptr<User> user)
{
    qDebug() << "UserFrameList::removeUser:" << user->path();
    foreach (auto w, m_loginWidgets) {
        if (w->uid() == user->uid()) {
            m_loginWidgets.removeOne(w);

            if (w == currentSelectedUser) {
                currentSelectedUser = nullptr;
            }
            w->deleteLater();
            break;
        }
    }

    //添加用户和删除用户时，重新计算区域大小
    updateLayout(width());
}

//点击用户
void UserFrameList::onUserClicked()
{
    UserWidget *widget = static_cast<UserWidget *>(sender());
    if (!widget) return;

    currentSelectedUser = widget;
    for (int i = 0; i != m_loginWidgets.size(); ++i) {
        if (m_loginWidgets[i]->isSelected()) {
            m_loginWidgets[i]->setFastSelected(false);
        }
    }
    emit clicked();
    emit requestSwitchUser(m_model->findUserByUid(widget->uid()));
}

/**
 * @brief 切换下一个用户
 */
void UserFrameList::switchNextUser()
{
    for (int i = 0; i != m_loginWidgets.size(); ++i) {
        if (m_loginWidgets[i]->isSelected()) {
            m_loginWidgets[i]->setSelected(false);
            if (i == (m_loginWidgets.size() - 1)) {
                currentSelectedUser = m_loginWidgets.first();
                currentSelectedUser->setSelected(true);
                //处理m_scrollArea翻页显示
                m_scrollArea->verticalScrollBar()->setValue(0);
                m_frameDataBind->updateValue("UserFrameList", currentSelectedUser->uid());
            } else {
                //处理m_scrollArea翻页显示
                int selectedRight = m_loginWidgets[i]->geometry().right();
                int scrollRight = m_scrollArea->widget()->geometry().right();
                if (selectedRight + UserFrameSpaceing == scrollRight) {
                    QPoint topLeft;
                    if (m_rowCount == 1) {
                        topLeft = m_loginWidgets[i + 1]->geometry().topLeft();
                    } else {
                        topLeft = m_loginWidgets[i]->geometry().topLeft();
                    }
                    m_scrollArea->verticalScrollBar()->setValue(topLeft.y());
                }

                currentSelectedUser = m_loginWidgets[i + 1];
                currentSelectedUser->setSelected(true);
                m_frameDataBind->updateValue("UserFrameList", currentSelectedUser->uid());
            }
            break;
        }
    }
}

/**
 * @brief 切换上一个用户
 */
void UserFrameList::switchPreviousUser()
{
    for (int i = 0; i != m_loginWidgets.size(); ++i) {
        if (m_loginWidgets[i]->isSelected()) {
            m_loginWidgets[i]->setSelected(false);
            if (i == 0) {
                currentSelectedUser = m_loginWidgets.last();
                currentSelectedUser->setSelected(true);
                //处理m_scrollArea翻页显示
                m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
                m_frameDataBind->updateValue("UserFrameList", currentSelectedUser->uid());
            } else {
                //处理m_scrollArea翻页显示
                QPoint topLeft = m_loginWidgets[i]->geometry().topLeft();
                if (topLeft.x() == 0) {
                    m_scrollArea->verticalScrollBar()->setValue(m_loginWidgets[i - 1]->geometry().topLeft().y());
                }

                currentSelectedUser = m_loginWidgets[i - 1];
                currentSelectedUser->setSelected(true);
                m_frameDataBind->updateValue("UserFrameList", currentSelectedUser->uid());
            }
            break;
        }
    }
}

void UserFrameList::onOtherPageChanged(const QVariant &value)
{
    foreach (auto w, m_loginWidgets) {
        w->setSelected(w->uid() == value.toUInt());
    }
}

void UserFrameList::updateLayout(int width)
{
    //处理窗体数量小于5个时的居中显示，取 窗体数量*窗体宽度 和 最大宽度 的较小值，设置为m_centerWidget的宽度
    auto userWidget = m_loginWidgets.constFirst();
    int userWidgetHeight = UserFrameHeight;
    if (userWidget)
        userWidgetHeight = userWidget->heightHint();

    // 根据界面总宽度计算每行可以显示多少用户信息，每行最多5个用户信息
    int count = 5;
    while ((UserFrameWidth + UserFrameSpaceing) * count > width - 10) {
        count--;
    }
    count = count <= 0 ? 1 : count;

    if (m_flowLayout->count() <= count) {
        m_scrollArea->setFixedSize((UserFrameWidth + UserFrameSpaceing) * m_flowLayout->count(), userWidgetHeight + 20);
    } else {
        m_scrollArea->setFixedSize((UserFrameWidth + UserFrameSpaceing) * count, (userWidgetHeight + UserFrameSpaceing) * 2);
    }

    m_centerWidget->setFixedWidth(m_scrollArea->width() - 10);

    std::shared_ptr<User> user = m_model->currentUser();
    if (user.get() == nullptr) return;
    for (auto it = m_loginWidgets.constBegin(); it != m_loginWidgets.constEnd(); ++it) {
        auto login_widget = *it;

        if (login_widget->uid() == user->uid()) {
            currentSelectedUser = login_widget;
            currentSelectedUser->setSelected(true);
        } else {
            login_widget->setSelected(false);
        }
    }
}

void UserFrameList::hideEvent(QHideEvent *event)
{
    return QWidget::hideEvent(event);
}

void UserFrameList::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        switchPreviousUser();
        break;
    case Qt::Key_Right:
        switchNextUser();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        for (auto it = m_loginWidgets.constBegin(); it != m_loginWidgets.constEnd(); ++it) {
            if ((*it)->isSelected()) {
                emit(*it)->clicked();
                break;
            }
        }
        break;
    case Qt::Key_Escape:
        emit clicked();
        emit requestSwitchUser(m_model->currentUser());
        break;
    default:
        break;
    }
    return QWidget::keyPressEvent(event);
}

void UserFrameList::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    if (currentSelectedUser != nullptr) {
        currentSelectedUser->setSelected(true);
    }
}

void UserFrameList::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    if (currentSelectedUser != nullptr)
        currentSelectedUser->setSelected(false);
}

void UserFrameList::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout(width());
}

void UserFrameList::OnDConfigPropertyChanged(const QString &key, const QVariant &value)
{
    if (key == SHOW_USER_NAME) {
        // 需要等待UserWidget处理完，延时100ms后更新布局
        QTimer::singleShot(100, this, [ = ]{
            updateLayout(width());
        });
    }
}
