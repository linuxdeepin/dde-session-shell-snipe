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
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent *event) override;

signals:

public slots:
    void updateTimeFormat(bool use24);

private:
    void initUi();
    void setTopTipText(const QString &text);
    void updateTopTipWidget();

private:
    QPointer<User> m_currentUser;
    TimeWidget *m_timeWidget;
    QLabel *m_topTip;
    QString m_tipText;
    QSpacerItem *m_topTipSpacer;
    QList<QMetaObject::Connection> m_currentUserConnects;
    DTK_CORE_NAMESPACE::DConfig *m_config;
};

#endif // CENTERTOPWIDGET_H
