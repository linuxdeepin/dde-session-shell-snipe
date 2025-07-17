#pragma once

#include <QWidget>

class ShutdownBlackWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShutdownBlackWidget(QWidget *parent = nullptr);

    void setBlackMode(const bool isBlackMode);

public Q_SLOTS:
    void setShutdownMode(const bool isBlackMode = false);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QCursor m_cursor;
};

// SPDX-FileCopyrightText: 2020 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
