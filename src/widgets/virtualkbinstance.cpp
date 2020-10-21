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
    qInfo() << "VirtualKBInstance::init";
    if (m_virtualKBWidget) {
        emit initFinished();
        return;
    }

    //初始化启动onborad进程
    if (!m_virtualKBProcess) {
        qInfo() << "create QProcess";
        m_virtualKBProcess = new QProcess(this);

        connect(m_virtualKBProcess, &QProcess::readyReadStandardOutput, this, &VirtualKBInstance::onReadyReadStandardOutput);
        connect(m_virtualKBProcess, SIGNAL(finished(int)), this, SLOT(onVirtualKBProcessFinished(int)));

        qInfo() << "start onboard";
        m_virtualKBProcess->start("onboard", QStringList() << "-e" << "--layout" << "Small");
    }
}

void VirtualKBInstance::stopVirtualKBProcess()
{
    //结束onborad进程
    if (m_virtualKBProcess) {
        m_virtualKBProcess->terminate();
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
    Q_UNUSED(exitCode);
    delete m_virtualKBProcess;
    m_virtualKBProcess = nullptr;
}

void VirtualKBInstance::onReadyReadStandardOutput()
{
    qInfo() << "read All Standard Output 111111";

    //启动完成onborad进程后，获取onborad主界面，将主界面显示在锁屏界面上
    QByteArray output = m_virtualKBProcess->readAllStandardOutput();

    if (output.isEmpty()) return;

    int xid = atoi(output.trimmed().toStdString().c_str());

    qInfo() << "read All Standard Output 2222" << xid;

    QWindow * w = QWindow::fromWinId(xid);

    qInfo() << "read All Standard Output 3333" << w;

    m_virtualKBWidget = QWidget::createWindowContainer(w);
    if (!m_virtualKBWidget) {
        return;
    }

    m_virtualKBWidget->setFixedSize(600, 200);
    m_virtualKBWidget->hide();

    qInfo() << "read All Standard Output initFinished" << w;

    QTimer::singleShot(300, [=] {
        emit initFinished();
    });
}
