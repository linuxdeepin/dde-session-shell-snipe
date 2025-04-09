// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INHIBITWARNVIEW_H
#define INHIBITWARNVIEW_H

#include "warningview.h"
#include "rounditembutton.h"
#include "sessionbasemodel.h"
#include "inhibitbutton.h"

#include <QWidget>

namespace Dtk {
namespace Widget {
class DSpinner;
}
}

class InhibitorRow : public QWidget
{
    Q_OBJECT
public:
    InhibitorRow(const QString &who, const QString &why, const QIcon &icon = QIcon(), QWidget *parent = nullptr);
    ~InhibitorRow() override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

class InhibitWarnView : public WarningView
{
    Q_OBJECT

public:
    explicit InhibitWarnView(SessionBaseModel::PowerAction inhibitType, QWidget *parent = nullptr);
    ~InhibitWarnView() override;

    struct InhibitorData {
        QString who;
        QString why;
        QString mode;
        quint32 pid;
        QString icon;
    };

    void setInhibitorList(const QList<InhibitorData> & list);
    void setInhibitConfirmMessage(const QString &text, bool showLoading = false);
    void setDelayView(bool showDelay);
    bool delayView() const;
    void setAcceptReason(const QString &reason) override;
    void setAcceptVisible(const bool acceptable);
    bool hasInhibit() const;

signals:
    void cancelled() const;
    void actionInvoked() const;

protected:
    QString iconString();
    bool focusNextPrevChild(bool next) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private:
    void initUi();
    void initConnection();

private slots:
    void onAccept();

private:
    SessionBaseModel::PowerAction m_inhibitType;
    QList<QWidget*> m_inhibitorPtrList;
    QVBoxLayout *m_inhibitorListLayout = nullptr;
    Dtk::Widget::DSpinner *m_loading;
    QLabel *m_confirmTextLabel = nullptr;
    InhibitButton *m_acceptBtn;
    InhibitButton *m_cancelBtn;
    QWidget *m_bottomWidget;
    int m_dataBindIndex;
    bool m_showDelay;
};

#endif // INHIBITWARNVIEW_H
