// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ROUND_POPUP_WIDGET_H
#define ROUND_POPUP_WIDGET_H

#include <QWidget>

class QVBoxLayout;

/*!
 * \brief The RoundPopupWidget class
 * 圆角弹窗页面
 */
class RoundPopupWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RoundPopupWidget(QWidget *parent = nullptr);

public:
    void setContent(QWidget *widget);
    QWidget *getContent() const;

protected:
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();

private:
    QWidget *m_pContentWidget;
    QVBoxLayout *m_mainLayout;
};


#endif // ROUND_POPUP_WIDGET_H
