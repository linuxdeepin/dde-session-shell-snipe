// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CENTERTOPWIDGET_H
#define CENTERTOPWIDGET_H

#include "userinfo.h"
#include "timewidget.h"

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
    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject* objPtr);

signals:

public slots:
    void updateTimeFormat(bool use24);


private:
    void initUi();
    void setTopTipText(const QString &text);
    void updateTopTipWidget();

#ifdef ENABLE_DSS_SNIPE
private slots:
    void onUserRegionFormatValueChanged(const QDBusMessage &dbusMessage);

private:
    QString getRegionFormatConfigPath(const User *user) const;
    QString getRegionFormatValue(const QString &userConfigDbusPath, const QString& key) const;
    QString getUserLocale(const User *user) const;
    void updateRegionFormatConnection(const User *user);
    void updateUserDateTimeFormat();
#endif // ENABLE_DSS_SNIPE

private:
    QPointer<User> m_currentUser;
    TimeWidget *m_timeWidget;
    QLabel *m_topTip;
    QString m_tipText;
    QSpacerItem *m_topTipSpacer;
    QList<QMetaObject::Connection> m_currentUserConnects;
};

#endif // CENTERTOPWIDGET_H
