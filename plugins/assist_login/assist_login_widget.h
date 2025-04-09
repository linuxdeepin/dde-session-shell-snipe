// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASSIST_LOGIN_WIDGET_H
#define ASSIST_LOGIN_WIDGET_H

#include <QWidget>
#include <DSpinner>
#include <DLabel>

namespace dss {

namespace module {

class AssistLoginWidget: public QWidget
{
    Q_OBJECT

public:
    explicit AssistLoginWidget(QWidget *parent = nullptr);

protected:
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

Q_SIGNALS:
    void sendAuth(const QString &account, const QString &token);
    void stopService();
    void startService();


private:
    void init();

    DTK_WIDGET_NAMESPACE::DSpinner *m_spinner;
    DTK_WIDGET_NAMESPACE::DLabel *m_label;
};
}
}

#endif //ASSIST_LOGIN_WIDGET_H
