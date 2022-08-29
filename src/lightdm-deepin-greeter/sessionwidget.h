// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONWIDGET_H
#define SESSIONWIDGET_H

#include <QFrame>
#include <QList>
#include <QSettings>

#include <QLightDM/SessionsModel>
#include <QLightDM/UsersModel>

#include "rounditembutton.h"
#include "framedatabind.h"

class SessionBaseModel;
class SessionWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SessionWidget(QWidget *parent = nullptr);
    void setModel(SessionBaseModel * const model);
    ~SessionWidget() override;

    void updateLayout();
    int sessionCount() const;
    const QString currentSessionKey() const;
    const QString currentSessionOwner() const { return m_currentUser; }

signals:
    void sessionChanged(const QString &sessionName);
    void hideFrame();

public slots:
    void switchToUser(const QString &userName);
    void leftKeySwitch();
    void rightKeySwitch();
    void chooseSession();

protected:
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent *event) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

private slots:
    void loadSessionList();
    void onSessionButtonClicked();

private:
    int sessionIndex(const QString &sessionName);
    void onOtherPageChanged(const QVariant &value);
    QString lastLoggedInSession(const QString &userName);

private:
    int m_currentSessionIndex;
    QString m_currentUser;
    SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;

    QLightDM::SessionsModel *m_sessionModel;
    QList<RoundItemButton *> m_sessionBtns;
    QLightDM::UsersModel *m_userModel;
    bool m_allowSwitchingToWayland;
    bool m_isWaylandExisted;
    QLabel *m_warningLabel;
    QString m_defaultSession;
};

#endif // SESSIONWIDGET_H
