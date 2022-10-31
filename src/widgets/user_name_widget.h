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
    void updateFullName(const QString &displayName);
    int heightHint() const;

public slots:
    void OnDConfigPropertyChanged(const QString &key, const QVariant &value);

private:
    void updateUserNameWidget();
    void updateDisplayNameWidget();

private:
    Dtk::Widget::DLabel *m_userPicLabel;                 // 小人儿图标
    Dtk::Widget::DLabel *m_fullNameLabel;                // 用户名
    Dtk::Widget::DLabel *m_displayNameLabel;        // 用户全名
    bool m_showUserName;                            // 是否显示账户名，记录DConfig的配置
    bool m_showDisplayName;                         // 是否显示全名
    QString m_fullNameStr;                          // 全名
    QString m_userNameStr;                          // 账户名
};
