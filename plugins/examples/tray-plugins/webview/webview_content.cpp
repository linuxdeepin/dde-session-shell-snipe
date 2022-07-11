/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
