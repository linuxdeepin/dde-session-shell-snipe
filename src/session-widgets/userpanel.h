// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERPANEL_H
#define USERPANEL_H

#include "actionwidget.h"

#include <QWidget>

const int UserPanelWidth = 330;
const int UserPanelHeight = 76;
const int UserAvatarSize = 56;

class QHBoxLayout;
class UserAvatar;

class UserPanel : public ActionWidget
{
    Q_OBJECT
public:
    explicit UserPanel(QWidget *parent = nullptr);

    inline uint uid() const { return m_uid; }
    void setUid(const uint uid);
    inline bool isSelected() const { return m_isSelected; }
    void setSelected(bool isSelected);
    void setAvatar(const QString &avatar);
    void setDisplayName(const QString &name);
    void setName(const QString &name);
    const QString &name() const { return m_name; }
    void setFullName(const QString &name);
    const QString &fullName() const { return m_fullName; }
    void setType(const QString &type);
    const QString &type() const { return m_type; }

private:
    void initUI();

private:
    bool m_isSelected;
    uint m_uid;
    QHBoxLayout *m_mainLayout;
    UserAvatar *m_avatar;
    QString m_fullName;
    QString m_name;
    QString m_type;
    QLabel *m_displayNameLabel;
    QLabel *m_typeLabel;
};

#endif // USERPANEL_H
