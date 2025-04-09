// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assist_login_widget.h"
#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QPushButton>

dss::module::AssistLoginWidget::AssistLoginWidget(QWidget *parent)
    : QWidget(parent)
    , m_spinner(nullptr)
    , m_label(nullptr)
{
    init();
}

void dss::module::AssistLoginWidget::init()
{
    setAccessibleName(QStringLiteral("LoginWidget"));
    setMinimumSize(260, 100);

    QVBoxLayout *layout = new QVBoxLayout();

    m_spinner = new DTK_WIDGET_NAMESPACE::DSpinner(this);
    m_spinner->setFixedSize(40, 40);
    m_spinner->start();
    layout->setSpacing(4);
    layout->addWidget(m_spinner, 0, Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
    m_label = new DTK_WIDGET_NAMESPACE::DLabel(this);
    m_label->setFixedSize(125, 40);
    m_label->setText(tr("Automatic login"));
    m_label->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
    Dtk::Widget::DFontSizeManager::instance()->bind(m_label, Dtk::Widget::DFontSizeManager::T6);
    layout->addWidget(m_label, 0, Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);

    setLayout(layout);
}

void dss::module::AssistLoginWidget::hideEvent(QHideEvent *event)
{
    Q_EMIT stopService();
    QWidget::hideEvent(event);
}
void dss::module::AssistLoginWidget::showEvent(QShowEvent *event)
{
    Q_EMIT startService();
    QWidget::showEvent(event);
}
