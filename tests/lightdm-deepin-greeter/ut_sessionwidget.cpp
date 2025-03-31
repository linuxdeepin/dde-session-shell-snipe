// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionwidget.h"

#include <QLabel>
#include <QTest>

#include <gtest/gtest.h>

class TstSessionWidget : public testing::Test
{
public:
    void SetUp() override;
    void TearDown() override;

public:
    SessionWidget *m_sessionwidget;
};

void TstSessionWidget::SetUp()
{
    m_sessionwidget = new SessionWidget();
}

void TstSessionWidget::TearDown()
{
    delete m_sessionwidget;
}

TEST_F(TstSessionWidget, BasicTest)
{
    //m_sessionwidget->show();
    m_sessionwidget->sessionCount();
    m_sessionwidget->currentSessionKey();
    m_sessionwidget->currentSessionOwner();
    m_sessionwidget->leftKeySwitch();
    m_sessionwidget->rightKeySwitch();
}

TEST_F(TstSessionWidget, EventTest)
{
    QTest::keyPress(m_sessionwidget, Qt::Key_Left);
    //m_sessionwidget->resizeEvent(new QResizeEvent(m_sessionwidget->size(), m_sessionwidget->size()));
    //m_sessionwidget->focusInEvent(new QFocusEvent(QEvent::FocusIn));
    //m_sessionwidget->focusOutEvent(new QFocusEvent(QEvent::FocusOut));
}
