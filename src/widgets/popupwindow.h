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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *e) override;
#else
    void enterEvent(QEvent *e) override;
#endif
    bool eventFilter(QObject *o, QEvent *e) override;

signals:
    void visibleChanged(bool visible);

private slots:
    void ensureRaised();

private:
    QPoint m_lastPoint;
};

#endif // POPUPWINDOW_H
