// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dconfig_helper.h"

#include <QWidget>

#include <DLabel>

class UserNameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserNameWidget(bool showDisplayName = true, QWidget *parent = nullptr);
    void initialize();
    void updateUserName(const QString &userName);
    void updateDisplayName(const QString &displayName);
    int heightHint() const;

public slots:
    void OnDConfigPropertyChanged(const QString &key, const QVariant &value);

private:
    Dtk::Widget::DLabel *m_userPic;              // 小人儿图标
    Dtk::Widget::DLabel *m_userName;             // 用户名
    Dtk::Widget::DLabel *m_displayNameLabel;     // 用户全名
    bool m_showDisplayName;
};
