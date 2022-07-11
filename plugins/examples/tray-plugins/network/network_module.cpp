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

#include "network_module.h"

#include <QWidget>
#include <QLabel>
#include <QIcon>

namespace dss {
namespace module {

NetworkModule::NetworkModule(QObject *parent)
    : QObject(parent)
    , m_networkWidget(nullptr)
    , m_tipLabel(nullptr)
    , m_itemLabel(nullptr)
{
    setObjectName(QStringLiteral("NetworkModule"));
}

NetworkModule::~NetworkModule()
{
    if (m_networkWidget) {
        delete m_networkWidget;
    }

    if (m_itemLabel) {
        delete m_itemLabel;
    }
}

void NetworkModule::init()
{
    initUI();

    m_itemLabel = new QLabel;
    m_itemLabel->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(20, 20));
    m_itemLabel->setFixedSize(20, 20);
}

QWidget *NetworkModule::itemWidget() const
{
    return m_itemLabel;
}

QWidget *NetworkModule::itemTipsWidget() const
{
    return m_tipLabel;
}

const QString NetworkModule::itemContextMenu() const
{
    QList<QVariant> items;
    items.reserve(2);

    QMap<QString, QVariant> shift;
    shift["itemId"] = "SHIFT";
    shift["itemText"] = tr("Turn on");
    shift["isActive"] = true;
    items.push_back(shift);

    QMap<QString, QVariant> settings;
    settings["itemId"] = "SETTINGS";
    settings["itemText"] = tr("Turn off");
    settings["isActive"] = true;
    items.push_back(settings);

    QMap<QString, QVariant> menu;
    menu["items"] = items;
    menu["checkableMenu"] = false;
    menu["singleCheck"] = false;
    return QJsonDocument::fromVariant(menu).toJson();
}

void NetworkModule::invokedMenuItem(const QString &menuId, const bool checked) const
{
    Q_UNUSED(checked)

    if (menuId == "SHIFT") {
        qDebug() << "shift clicked";
    } else if (menuId == "SETTINGS") {
        qDebug() << "settings clicked";
    }
}

void NetworkModule::initUI()
{
    if (m_networkWidget) {
        return;
    }
    m_networkWidget = new QWidget;
    m_networkWidget->setAccessibleName(QStringLiteral("NetworkWidget"));
    m_networkWidget->setStyleSheet("background-color: red");
    m_networkWidget->setFixedSize(200, 100);

    m_tipLabel = new QLabel("network info");
}

} // namespace module
} // namespace dss
