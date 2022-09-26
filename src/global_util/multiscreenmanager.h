// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MULTISCREENMANAGER_H
#define MULTISCREENMANAGER_H

#include <QObject>
#include <QWidget>
#include <QScreen>
#include <QMap>
#include <functional>
#include <QTimer>
#include <com_deepin_system_systemdisplay.h>

using SystemDisplayInter = com::deepin::system::Display;

const static int COPY_MODE = 1;
const static int EXTENDED_MODE = 2;

class MultiScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit MultiScreenManager(QObject *parent = nullptr);

    void register_for_mutil_screen(std::function<QWidget* (QScreen *, int)> function);
    void startRaiseContentFrame();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void dealScreenAdded(QPointer<QScreen> screen); // 添加自定义的信号和槽是为了规避直接处理qt信号导致的崩溃问题
    void dealScreenRemoved(QPointer<QScreen> screen);
    void onScreenAdded(QPointer<QScreen> screen);
    void onScreenRemoved(QPointer<QScreen> screen);
    void raiseContentFrame();
    int getDisplayModeByConfig(const QString &config) const;

signals:
    void screenAddSignal(QPointer<QScreen> screen);
    void screenRemoveSignal(QPointer<QScreen> screen);

private slots:
    void onDisplayModeChanged(const QString &);
    void checkLockFrameLocation();

private:
    std::function<QWidget* (QPointer<QScreen> , int)> m_registerFunction;
    QMap<QPointer<QScreen>, QPointer<QWidget>> m_frames;
    QTimer *m_raiseContentFrameTimer;
    SystemDisplayInter *m_systemDisplay;
    bool m_isCopyMode;
};

#endif // MULTISCREENMANAGER_H
