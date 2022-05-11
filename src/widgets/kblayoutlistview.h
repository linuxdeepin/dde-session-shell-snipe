/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#ifndef KBLAYOUTLISTVIEW
#define KBLAYOUTLISTVIEW

#include <DListView>

#include "constants.h"
#include "util_updateui.h"
#include "xkbparser.h"

DWIDGET_USE_NAMESPACE

class KBLayoutListView: public DListView
{
    Q_OBJECT

public:
    KBLayoutListView(const QString &language = QString(), QWidget *parent = nullptr);
    ~KBLayoutListView() override;

    void initData(const QStringList &buttons);
    void updateList(const QString &str);

signals:
    void itemClicked(const QString &str);

public slots:
    void onItemClick(const QModelIndex &index);

private:
    void initUI();
    void updateSelectState(const QString &name);
    void addItem(const QString &name);

private:
    QStringList m_buttons;
    XkbParser *m_xkbParse;
    QStringList m_kbdParseList;

    QStandardItemModel *m_buttonModel;
    QString m_curLanguage;
    bool m_clickState;
};
#endif // KBLAYOUTLISTVIEW
