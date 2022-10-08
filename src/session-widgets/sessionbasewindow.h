// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONBASEWINDOW_H
#define SESSIONBASEWINDOW_H

#include <QFrame>
#include <QVBoxLayout>
#include <QPointer>
#include <QSpacerItem>

class SessionBaseWindow : public QFrame
{
    Q_OBJECT
public:
    explicit SessionBaseWindow(QWidget *parent = nullptr);
    virtual void setLeftBottomWidget(QWidget *const widget) final;
    virtual void setCenterBottomWidget(QWidget *const widget) final;
    virtual void setRightBottomWidget(QWidget *const widget) final;
    virtual void setCenterContent(QWidget *const widget, int stretch = 0, Qt::Alignment align = Qt::Alignment(), int spacerHeight = 0) final;
    virtual void setCenterTopWidget(QWidget *const widget) final;
    QSize getCenterContentSize();

protected:
    void setTopFrameVisible(bool visible);
    void setBottomFrameVisible(bool visible);
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    int calcCurrentHeight(int height) const;
    int calcTopSpacing(int authWidgetTopSpacing) const;

private:
    void initUI();

protected:
    QFrame *m_centerTopFrame;
    QFrame *m_centerFrame;
    QFrame *m_bottomFrame;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_centerTopLayout;
    QHBoxLayout *m_centerLayout;
    QVBoxLayout *m_centerVLayout;
    QHBoxLayout *m_leftBottomLayout;
    QHBoxLayout *m_centerBottomLayout;
    QHBoxLayout *m_rightBottomLayout;
    QWidget *m_centerTopWidget;
    QPointer<QWidget> m_centerWidget;
    QWidget *m_leftBottomWidget;
    QWidget *m_centerBottomWidget;
    QWidget *m_rightBottomWidget;
    QSpacerItem *m_centerSpacerItem;
};

#endif // SESSIONBASEWINDOW_H
