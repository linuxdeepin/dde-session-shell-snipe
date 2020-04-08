#ifndef DBUSLOCKAGENT_H
#define DBUSLOCKAGENT_H

#include <QObject>

class SessionBaseModel;
class DBusLockAgent : public QObject
{
    Q_OBJECT
public:
    explicit DBusLockAgent(QObject *parent = nullptr);
    void setModel(SessionBaseModel *const model);

    void Show();
    void ShowUserList();
    void ShowAuth(bool active);
    void ShowBlack();

private:
    void showFrame();

private:
    SessionBaseModel *m_model;
};

#endif // DBUSLOCKAGENT_H
