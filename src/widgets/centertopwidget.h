// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CENTERTOPWIDGET_H
#define CENTERTOPWIDGET_H

#include "userinfo.h"
#include "timewidget.h"

#include <DConfig>

#include <QWidget>
#include <QPointer>
#include <QLabel>
#include <QSpacerItem>

class CenterTopWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CenterTopWidget(QWidget *parent = nullptr);
    void setCurrentUser(User *user);

signals:

public slots:
    void updateTimeFormat(bool use24);

private:
    void initUi();

private:
    QPointer<User> m_currentUser;
    TimeWidget *m_timeWidget;
    QLabel *m_topTip;
    QSpacerItem *m_topTipSpacer;
    QList<QMetaObject::Connection> m_currentUserConnects;
    DTK_CORE_NAMESPACE::DConfig *m_config;
};

#endif // CENTERTOPWIDGET_H
