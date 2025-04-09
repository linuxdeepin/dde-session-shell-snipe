// SPDX-FileCopyrightText: 2018 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DDE_SESSION_SHELL_BACKGROUNDHANDLERTHREAD_H
#define DDE_SESSION_SHELL_BACKGROUNDHANDLERTHREAD_H

#include <QSize>
#include <QThread>
#include <QPixmap>

class BackgroundHandlerThread : public QThread
{
    Q_OBJECT
public:
    explicit BackgroundHandlerThread(QObject *parent = nullptr);

    void setBackgroundInfo(const QString &path, const QSize &size, const qreal &devicePixelRatioF);

    // 直接调用handle则不会通过线程处理图片
    void handle();

signals:
    void backgroundHandled(const QPixmap &pixmap);

protected:
    void run() override;

private:
    static QPixmap handleBackground(const QString &path, const QSize &size, const qreal &devicePixelRatioF);

private:
    QString m_path;
    QSize m_targetSize;
    qreal m_devicePixelRatioF;
};

#endif //DDE_SESSION_SHELL_BACKGROUNDHANDLERTHREAD_H
