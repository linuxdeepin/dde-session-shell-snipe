// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modulewidget.h"
#include "waypointmodel.h"
#include "translastiondoc.h"

#include <DHiDPIHelper>
#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QPalette>
#include <QTimer>

using namespace gestureLogin;

// 文案、图片与按钮
// 考虑到
ModuleWidget::ModuleWidget(QWidget *parent)
    : QWidget(parent)
    , m_title(nullptr)
    , m_tip(nullptr)
    , m_firstEnroll(nullptr)
    , m_mainLayout(nullptr)
    , m_startEnrollBtn(nullptr)
    , m_inputWid(nullptr)
    , m_layout(nullptr)
{
    this->setContentsMargins(0, 0, 0, 0);

    init();
    initConnection();
}

void ModuleWidget::reset()
{
}

void ModuleWidget::init()
{
    DGUI_USE_NAMESPACE

    auto model = WayPointModel::instance();
    m_title = new DLabel;
    m_title->setFixedHeight(35);

    m_tip = new DTipLabel;
    if (model->appType() == ModelAppType::LoginLock) {
        m_tip->setForegroundRole(QPalette::ColorRole::Light);
    }

    m_tip->setFixedHeight(26);
    m_startEnrollBtn = new DPushButton;
    m_startEnrollBtn->setFixedSize(200, 54);

    m_title->setVisible(false);
    m_tip->setVisible(false);
    m_startEnrollBtn->setVisible(false);

    DFontSizeManager::instance()->bind(m_title, DFontSizeManager::T3);
    DFontSizeManager::instance()->bind(m_tip, DFontSizeManager::T5);

    m_iconLabel = new DLabel;
    m_iconLabel->setVisible(false);

    m_inputWid = new GesturePannel;
    m_inputWid->setVisible(false);

    m_layout = new QVBoxLayout;
    this->setLayout(m_layout);

    if (model->getGestureState() == GestureState::NotSet && model->appType() == ModelAppType::LoginLock) {
        showFirstEnroll();
    } else {
        showUserInput();
    }
}

void ModuleWidget::initConnection()
{
    auto model = WayPointModel::instance();

    connect(model, &WayPointModel::titleChanged, m_title, [&](const QString &text) {
        m_title->setVisible(true);
        m_title->setEnabled(true);
        m_title->setText(text);
    });
    connect(model, &WayPointModel::tipChanged, m_tip, [&](const QString &text) {
        m_tip->setVisible(true);
        m_tip->setEnabled(true);
        m_tip->setText(text);
    });
}

void ModuleWidget::showFirstEnroll()
{
    DWIDGET_USE_NAMESPACE

    int count = m_layout->count();
    if (count) {
        while (count != -1) {
            m_layout->takeAt(--count);
        }
    }

    // 首次录入的布局
    m_layout->addStretch(50);
    m_layout->addWidget(m_title, 0, Qt::AlignCenter);
    m_layout->addSpacing(10);
    m_layout->addWidget(m_tip, 0, Qt::AlignCenter);
    m_layout->addSpacing(60);

    QPixmap iconPix = DHiDPIHelper::loadNxPixmap(":/icons/firstEnroll.svg");
    auto scaledPixmap = iconPix.scaled(120, 120, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_iconLabel->setPixmap(scaledPixmap);
    m_layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);
    m_layout->addSpacing(130);

    m_layout->addWidget(m_startEnrollBtn, 0, Qt::AlignCenter);

    m_title->setVisible(true);
    m_tip->setVisible(true);
    m_startEnrollBtn->setVisible(true);
    m_iconLabel->setVisible(true);

    m_layout->addStretch(100);

    // 固定文本，不参与动态变化
    auto trans = TranslastionDoc::instance();
    m_title->setText(trans->getDoc(DocIndex::Enabled));
    m_tip->setText(trans->getDoc(DocIndex::RequestFirstEnroll));
    m_startEnrollBtn->setText(trans->getDoc(DocIndex::SetPasswd));

    connect(m_startEnrollBtn, &DPushButton::clicked, this, [this] {
        showUserInput();
    });
}

void ModuleWidget::showUserInput()
{
    int count = m_layout->count();
    if (count) {
        while (count != -1) {
            m_layout->takeAt(--count);
        }
    }

    m_startEnrollBtn->setVisible(false);
    m_iconLabel->setVisible(false);

    m_layout->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_title, 0, Qt::AlignCenter);
    m_layout->addSpacing(10);
    m_layout->addWidget(m_tip, 0, Qt::AlignCenter);
    m_layout->addSpacing(40);
    m_layout->addWidget(m_inputWid, 0, Qt::AlignCenter);

    m_title->setVisible(true);
    m_tip->setVisible(true);
    m_inputWid->setVisible(true);

    m_title->setText(WayPointModel::instance()->title());
    m_tip->setText(WayPointModel::instance()->tip());

    m_layout->addStretch(100);
}
