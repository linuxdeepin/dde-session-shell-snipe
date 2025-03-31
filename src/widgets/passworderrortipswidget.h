// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PASSWORDTIPSWIDGET_H
#define PASSWORDTIPSWIDGET_H

#include <DLabel>
#include <DTextEdit>
#include <DBackgroundGroup>

#include "QWidget"
#include "QLabel"
#include "QPushButton"
#include "QScrollArea"

DWIDGET_USE_NAMESPACE

class PasswordErrorTipsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PasswordErrorTipsWidget(QWidget *parent = nullptr);
    void initUi();

    void reset();
    void setErrDetailVisible(bool isVisible);
    void addErrorDetailMsg(const QString& msg);
    void setErrorTips(const QString &tips);
    bool hasErrorTips();

protected:
    bool eventFilter(QObject *watch, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    DLabel* m_tipLabel;
    DLabel* m_showMore;
    DTextEdit* m_detailTextEdit;
    QWidget* m_tipsWidget;

    int m_errorMsgNum;
};


#endif //PASSWORDTIPSWIDGET_H
