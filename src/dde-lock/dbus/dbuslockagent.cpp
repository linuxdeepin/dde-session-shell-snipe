#include "dbuslockagent.h"

#include "src/session-widgets/sessionbasemodel.h"

DBusLockAgent::DBusLockAgent(QObject *parent) : QObject(parent)
{

}

void DBusLockAgent::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

void DBusLockAgent::Show()
{
    m_model->setIsBlackModel(false);
    m_model->setIsHibernateModel(false);
    showFrame();
}

void DBusLockAgent::ShowAuth(bool active)
{
    Show();
    emit m_model->activeAuthChanged(!active);
}

// 待机，enable=true：进入待机；enable=false：待机恢复
void DBusLockAgent::Suspend(bool enable)
{
    if (enable) {
        m_model->setIsBlackModel(true);
        showFrame();
    } else {
        QDBusInterface infc("com.deepin.daemon.Power","/com/deepin/daemon/Power","com.deepin.daemon.Power");
        // 待机恢复需要密码
        bool bSuspendLock = infc.property("SleepLock").toBool();

        if (bSuspendLock) {
            m_model->setIsBlackModel(false);
            showFrame();
        } else {
            m_model->setIsShow(false);
            emit m_model->visibleChanged(false);
        }
    }
}

void DBusLockAgent::Hibernate(bool enable)
{
    m_model->setIsHibernateModel(enable);
    showFrame();
}

void DBusLockAgent::ShowUserList()
{
    emit m_model->showUserList();
}

void DBusLockAgent::showFrame()
{
    m_model->setIsShow(true);

    emit m_model->visibleChanged(true);
}

