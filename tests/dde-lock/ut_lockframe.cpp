#include "lockworker.h"
#include "sessionbasemodel.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

class UT_LockFrame : public testing::Test
{
protected:
    void SetUp();
    void TearDown();

    SessionBaseModel *m_sessionBaseModel;
    LockWorker *m_lockWorker;
};

void UT_LockFrame::SetUp()
{
    m_sessionBaseModel = new SessionBaseModel(SessionBaseModel::AuthType::LockType);
    m_lockWorker = new LockWorker(m_sessionBaseModel);
}

void UT_LockFrame::TearDown()
{
    delete m_sessionBaseModel;
    delete m_lockWorker;
}

TEST_F(UT_LockFrame, init)
{

}
