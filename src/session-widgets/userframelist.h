// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERFRAMELIST_H
#define USERFRAMELIST_H

#include <dflowlayout.h>

#include <QMap>
#include <QPointer>

#include <memory>

class UserWidget;
class User;
class SessionBaseModel;
class FrameDataBind;
class QVBoxLayout;
class QScrollArea;

DWIDGET_USE_NAMESPACE

class UserFrameList : public QWidget
{
    Q_OBJECT
public:
    explicit UserFrameList(QWidget *parent = nullptr);
    void setModel(SessionBaseModel *model);
    void setFixedSize(const QSize &size);
    void updateLayout(int width);

signals:
    void requestSwitchUser(std::shared_ptr<User> user);
    void clicked();

public slots:
    void OnDConfigPropertyChanged(const QString &key, const QVariant &value);

protected:
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();
    void handlerBeforeAddUser(std::shared_ptr<User> user);
    void addUser(const std::shared_ptr<User> user);
    void removeUser(const std::shared_ptr<User> user);
    void onUserClicked();
    void switchNextUser();
    void switchPreviousUser();
    void onOtherPageChanged(const QVariant &value);

private:
    QScrollArea *m_scrollArea;
    DFlowLayout *m_flowLayout;
    QList<UserWidget *> m_loginWidgets;
    QMap<uid_t, UserWidget *> m_userWidgets;
    QPointer<UserWidget> currentSelectedUser;
    SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;
    QWidget *m_centerWidget;
    int m_colCount;
    int m_rowCount;
};

#endif // USERFRAMELIST_H
