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

#ifndef WEBVIEWCONTENT_H
#define WEBVIEWCONTENT_H

#include <QWidget>
#include <QtWebEngineWidgets/qwebengineview.h>

class QVBoxLayout;

namespace dss {
namespace module {

class WebviewContent : public QWidget
{
    Q_OBJECT

public:
    explicit WebviewContent(QWidget *parent = nullptr);

private:
    void initUI();

private:
    QVBoxLayout *m_mainLayout;
    QWebEngineView *m_webview;
};

} // namespace module
} // namespace dss
#endif // WEBVIEWCONTENT_H
