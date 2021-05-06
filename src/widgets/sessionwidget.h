/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SESSIONWIDGET_H
#define SESSIONWIDGET_H

#include <QFrame>
#include <QList>
#include <QSettings>

#include <QLightDM/SessionsModel>

#include "rounditembutton.h"
#include "src/session-widgets/framedatabind.h"

class SessionBaseModel;
class SessionWidget : public QFrame
{
    Q_OBJECT
public:
    explicit SessionWidget(QWidget *parent = nullptr);
    static SessionWidget *getInstance(QWidget *parent = nullptr);
    void setModel(SessionBaseModel *const model);
    ~SessionWidget() override;

    void show();
    int sessionCount() const;
    const QString lastSessionName() const;
    const QString currentSessionName() const;
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

private slots:
    void loadSessionList();
    void onSessionButtonClicked();

private:
    int sessionIndex(const QString &sessionName);
    void onOtherPageChanged(const QVariant &value);

private:
    int m_currentSessionIndex;
    QString m_currentUser;
    SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;

    QLightDM::SessionsModel *m_sessionModel;
    QList<RoundItemButton *> m_sessionBtns;
};

#endif // SESSIONWIDGET_H
