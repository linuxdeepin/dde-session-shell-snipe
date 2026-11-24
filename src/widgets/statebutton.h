// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STATEBUTTON_H
#define STATEBUTTON_H

#include <QPushButton>

class StateButton : public QPushButton
{
    Q_OBJECT
public:
    explicit StateButton(const QString &text, QWidget *parent = nullptr);
    ~StateButton() override;

    void setNormalTextColor(const QColor &color);
    void setHoverTextColor(const QColor &color);
    void setPressTextColor(const QColor &color);

    void setNormalBackground(const QColor &color);
    void setHoverBackground(const QColor &color);
    void setPressBackground(const QColor &color);

    void setBorderRadius(int radius);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateTextColor();
    void updateBackground();

    QColor m_normalTextColor;
    QColor m_hoverTextColor;
    QColor m_pressTextColor;

    QColor m_normalBackground;
    QColor m_hoverBackground;
    QColor m_pressBackground;

    int m_borderRadius;
    bool m_hovered;
    bool m_pressed;
};

#endif // STATEBUTTON_H
