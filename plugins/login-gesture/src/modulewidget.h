// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include "gesturepannel.h"

#include <DLabel>
#include <DTipLabel>
#include <DPushButton>

#include <QWidget>
#include <QLayout>

DWIDGET_USE_NAMESPACE

namespace gestureLogin {
class ModuleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleWidget(QWidget *parent = nullptr);
    void reset();

public Q_SLOTS:
    void showUserInput();

private:
    void init();
    void initConnection();
    void showFirstEnroll(); // 登录锁屏上首次设置

private:
    // 大标题与小标题
    DLabel *m_title;
    DLabel *m_tip;
    QLayout *m_firstEnroll;
    QLayout *m_mainLayout;
    DPushButton *m_startEnrollBtn;
    DLabel *m_iconLabel;
    GesturePannel *m_inputWid;
    QVBoxLayout *m_layout;
};
} // namespace gestureLogin
#endif // LOGINWIDGET_H
