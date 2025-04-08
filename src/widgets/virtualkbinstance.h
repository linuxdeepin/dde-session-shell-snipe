// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIRTUALKBINSTANCE_H
#define VIRTUALKBINSTANCE_H

#include <QObject>

class QProcess;
class VirtualKBInstance : public QObject
{
    Q_OBJECT
public:
    static VirtualKBInstance &Instance();
    QWidget *virtualKBWidget();
    ~VirtualKBInstance() override;

    void init();
    void stopVirtualKBProcess();

public slots:
    void onVirtualKBProcessFinished(int exitCode);

signals:
    void initFinished();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    VirtualKBInstance(QObject *parent = nullptr);
    VirtualKBInstance(const VirtualKBInstance &) = delete;

private:
    QWidget *m_virtualKBWidget;
    QProcess * m_virtualKBProcess = nullptr;
};

#endif // VIRTUALKBINSTANCE_H
