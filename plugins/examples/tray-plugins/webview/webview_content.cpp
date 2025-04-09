// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webview_content.h"

#include <QBoxLayout>

namespace dss {
namespace module {

WebviewContent::WebviewContent(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_webview(nullptr)
{
    setObjectName(QStringLiteral("WebviewContent"));
    setAccessibleName(QStringLiteral("WebviewContent"));

    setFixedSize(600, 500);

    initUI();
}

void WebviewContent::initUI()
{
    m_mainLayout = new QVBoxLayout(this);

    m_webview = new QWebEngineView(this);
    m_webview->load(QUrl("http://detectportal.deepin.com"));

    m_mainLayout->addWidget(m_webview);
}

} // namespace module
} // namespace dss
