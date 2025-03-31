// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef POPUPWINDOW_H
#define POPUPWINDOW_H

#include <DArrowRectangle>

class PopupWindow : public Dtk::Widget::DArrowRectangle
{
    Q_OBJECT

public:
    explicit PopupWindow(QWidget *parent = 0);
    ~PopupWindow();

    bool model() const;

    void setContent(QWidget *content);

    using Dtk::Widget::DArrowRectangle::show;
    void show(const QPoint &pos);
    void toggle(const QPoint &pos);
    void hide();

protected:
    void showEvent(QShowEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

signals:
    void visibleChanged(bool visible);

private slots:
    void ensureRaised();

private:
    QPoint m_lastPoint;
};

#endif // POPUPWINDOW_H
