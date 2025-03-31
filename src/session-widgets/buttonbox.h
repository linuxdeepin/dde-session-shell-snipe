// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BUTTONBOX_H
#define BUTTONBOX_H

#include <DStyle>

#include <QButtonGroup>
#include <QAbstractButton>

DWIDGET_USE_NAMESPACE
class QHBoxLayout;
class ButtonBoxButton : public QAbstractButton
{
    Q_OBJECT
public:
    ButtonBoxButton(const QIcon& icon, const QString &text = QString(), QWidget *parent = nullptr);
    ButtonBoxButton(DStyle::StandardPixmap iconType = static_cast<DStyle::StandardPixmap>(-1),
                     const QString &text = QString(), QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);
    void setIcon(DStyle::StandardPixmap iconType);
    void setRadius(int radius);
    void setLeftRoundedEnabled(bool enabled);
    void setRightRoundedEnabled(bool enabled);
    void setVisibleAttr(bool visible) { m_visible = visible; }
    inline bool testVisibleAttr() const { return m_visible; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_radius;
    bool m_leftRoundEnabled;
    bool m_rightRoundEnabled;
    bool m_visible; // 标识 button 是否需要显示
};

//ButtonBox类
class ButtonBox : public QWidget
{
    Q_OBJECT
public:
    ButtonBox(QWidget *parent = nullptr);

    void setButtonList(const QList<ButtonBoxButton*> &list, bool checkable);
    inline QList<QAbstractButton *> buttonList() const { return m_group->buttons(); }
    void removeButton(ButtonBoxButton* button);
    QList<ButtonBoxButton *> buttonBoxList() const;
    inline QAbstractButton *checkedButton() const { return m_group->checkedButton(); }
    inline QAbstractButton *button(int id) const { return m_group->button(id); }
    inline void setId(QAbstractButton *button, int id) { m_group->setId(button, id); }
    inline int checkedId() const { return m_group->checkedId(); }
    inline void setATCustomBtnHide(bool btnHide) { m_atCustonBtnHide = btnHide; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QButtonGroup *m_group;
    QHBoxLayout *m_layout;
    bool m_atCustonBtnHide;
};
#endif // BUTTONBOX_H
