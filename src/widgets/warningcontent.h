// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WARNINGCONTENT_H
#define WARNINGCONTENT_H

#include <QObject>
#include <QWidget>

#include "sessionbasewindow.h"
#include "sessionbasemodel.h"
#include "warningview.h"
#include "inhibitwarnview.h"
#include "multiuserswarningview.h"
#include "dbus/dbuslogin1manager.h"

class WarningContent : public SessionBaseWindow
{
    Q_OBJECT

public:
    explicit WarningContent(QWidget *parent = nullptr);
    ~WarningContent() override;
    static WarningContent *instance();
    void setModel(SessionBaseModel * const model);
    void beforeInvokeAction(bool needConfirm);
    void setPowerAction(const SessionBaseModel::PowerAction action);
    bool supportDelayOrWait() const;
    void tryGrabKeyboard(bool exitIfFailed = true);

signals:
    void requestLockFrameHide();

protected:
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    QList<InhibitWarnView::InhibitorData> listInhibitors(const SessionBaseModel::PowerAction action);
    void doCancelShutdownInhibit();
    void doAcceptShutdownInhibit();

private slots:
    void shutdownInhibit(const SessionBaseModel::PowerAction action, bool needConfirm);

private:
    SessionBaseModel *m_model;
    DBusLogin1Manager *m_login1Inter;
    WarningView * m_warningView = nullptr;
    QStringList m_inhibitorBlacklists;
    SessionBaseModel::PowerAction m_powerAction;
    int m_failures;
};

class InhibitHint
{
public:
    QString name, icon, why;

    friend const QDBusArgument &operator>>(const QDBusArgument &argument, InhibitHint &obj)
    {
        argument.beginStructure();
        argument >> obj.name >> obj.icon >> obj.why;
        argument.endStructure();
        return argument;
    }
};

#endif // WARNINGCONTENT_H
