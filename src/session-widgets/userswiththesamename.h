// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERSWITHTHESAMENAME_H
#define USERSWITHTHESAMENAME_H

#include "inhibitbutton.h"

#include <QPointer>

class UserPanel;
class SessionBaseModel;
class QVBoxLayout;

class UsersWithTheSameName : public QWidget
{
    Q_OBJECT
public:
    explicit UsersWithTheSameName(SessionBaseModel *model, QWidget *parent = nullptr);
    bool findUsers(const QString nativeUserName, const QString &doMainAccountDetail);

signals:
    void requestCheckAccount(const QString &account, bool switchUser = true);
    void clicked();

protected:
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void initUI();
    void addUserPanel(QList<UserPanel *> &list);
    void onUserClicked();
    void switchNextUser();
    void switchPreviousUser();
    void updateCurrentSelectedUser(bool isMouseEner);
    void clearUserPanelList();

private:
    QVBoxLayout *m_panelLayout;
    QList<UserPanel *> m_loginWidgets;
    QPointer<UserPanel> m_currentSelectedUser;
    SessionBaseModel *m_model;
    QWidget *m_centerWidget;
    InhibitButton *m_backButton;
};

#endif // USERSWITHTHESAMENAME_H
