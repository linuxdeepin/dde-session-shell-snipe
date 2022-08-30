// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "virtualkbinstance.h"

#include <QProcess>
#include <QWindow>
#include <QWidget>
#include <QTimer>
#include <QDebug>
#include <QResizeEvent>

VirtualKBInstance &VirtualKBInstance::Instance()
{
    static VirtualKBInstance virtualKB;
    return virtualKB;
}

QWidget *VirtualKBInstance::virtualKBWidget() {
    return m_virtualKBWidget;
}

VirtualKBInstance::~VirtualKBInstance()
{
    stopVirtualKBProcess();
}

void VirtualKBInstance::init()
{
    if (m_virtualKBWidget) {
        emit initFinished();
        return;
    }

    //初始化启动onborad进程
    if (!m_virtualKBProcess) {
        m_virtualKBProcess = new QProcess(this);

        connect(m_virtualKBProcess, &QProcess::readyReadStandardOutput, [ = ]{
            //启动完成onborad进程后，获取onborad主界面，将主界面显示在锁屏界面上
            QByteArray output = m_virtualKBProcess->readAllStandardOutput();

            if (output.isEmpty()) return;

            int xid = atoi(output.trimmed().toStdString().c_str());
            QWindow *w = QWindow::fromWinId(static_cast<WId>(xid));
            m_virtualKBWidget = QWidget::createWindowContainer(w);
            m_virtualKBWidget->setAccessibleName("VirtualKBWidget");
            m_virtualKBWidget->setFixedSize(600, 200);
            m_virtualKBWidget->hide();

            QTimer::singleShot(300, [=] {
                emit initFinished();
            });
        });
        connect(m_virtualKBProcess, SIGNAL(finished(int)), this, SLOT(onVirtualKBProcessFinished(int)));

        m_virtualKBProcess->start("onboard", QStringList() << "-e" << "--layout" << "Small" << "--size" << "60x5" << "-a");
    }
}

void VirtualKBInstance::stopVirtualKBProcess()
{
    //结束onborad进程
    if (m_virtualKBProcess) {
        m_virtualKBProcess->close();
        m_virtualKBWidget = nullptr;
    }
}

bool VirtualKBInstance::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_virtualKBWidget && event->type() == QEvent::Resize) {
        QResizeEvent *e = static_cast<QResizeEvent*>(event);
        return e->size() != QSize(600, 200);
    }

    return QObject::eventFilter(watched, event);
}

VirtualKBInstance::VirtualKBInstance(QObject *parent)
    : QObject(parent)
    , m_virtualKBWidget(nullptr)
{
}

void VirtualKBInstance::onVirtualKBProcessFinished(int exitCode)
{
    Q_UNUSED(exitCode)
    m_virtualKBProcess = nullptr;
}
