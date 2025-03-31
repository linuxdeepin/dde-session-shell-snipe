// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dconfig_helper.h"

#include <QWidget>
#include <QResizeEvent>

#include <DLabel>

class UserNameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserNameWidget(bool respondFontSizeChange, bool showDisplayName, QWidget *parent = nullptr);
    void initialize();
    void updateUserName(const QString &userName);
    void updateFullName(const QString &displayName);
    void setDomainUserVisible(const bool visible);
    int heightHint() const;
    void resizeEvent(QResizeEvent *event) override;
    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

private:
    void updateUserNameWidget();
    void updateDisplayNameWidget();

private:
    Dtk::Widget::DLabel *m_userPicLabel;            // 小人儿图标
    Dtk::Widget::DLabel *m_fullNameLabel;           // 用户名
    Dtk::Widget::DLabel *m_displayNameLabel;        // 用户全名
    Dtk::Widget::DLabel *m_domainUserLabel;         // 域账户标识
    bool m_showUserName;                            // 是否显示账户名，记录DConfig的配置
    bool m_showDisplayName;                         // 是否显示全名
    QString m_fullNameStr;                          // 全名
    QString m_userNameStr;                          // 账户名
    bool m_respondFontSizeChange;                   // 是否响应字体变化
};
