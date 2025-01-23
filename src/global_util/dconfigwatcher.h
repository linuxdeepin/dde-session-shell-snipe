// SPDX-FileCopyrightText: 2020 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DCONFIGWATCHER_H
#define DCONFIGWATCHER_H

#include <DConfig>

const static int Status_Enabled = 0;
const static int Status_Disabled = 1;
const static int Status_Hidden = 2;

class DConfigWatcher : public QObject
{
    Q_OBJECT
public:
    static DConfigWatcher *instance();

    void bind(const QString &key, QWidget *binder);
    void erase(const QString &key);
    void erase(const QString &key, QWidget *binder);
    const int getStatus(const QString &key);

signals:
    void enableChanged(const QString &key, bool enable);

private:
    DConfigWatcher(QObject *parent = nullptr);

    void setStatus(const QString &key, QWidget *binder);
    void onStatusModeChanged(const QString &key);

private:
    QMultiHash<QString, QWidget *> m_map;
    Dtk::Core::DConfig *m_dConfig = nullptr;
};

#endif // DCONFIGWATCHER_H
