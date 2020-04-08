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
    showFrame();
}

void DBusLockAgent::ShowAuth(bool active)
{
    Show();
    emit m_model->activeAuthChanged(!active);
}

void DBusLockAgent::ShowUserList()
{
    emit m_model->showUserList();
}

void DBusLockAgent::ShowBlack()
{
    m_model->setIsBlackModel(true);
    showFrame();
}

void DBusLockAgent::showFrame()
{
    m_model->setIsShow(true);

    emit m_model->visibleChanged(true);
}
