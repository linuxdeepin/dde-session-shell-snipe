/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
*
* Author:     fengliyu <fengliyu@uniontech.com>
* Maintainer:  fengliyu <fengliyu@uniontech.com>
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DModifyDialog_H
#define DModifyDialog_H

#include <DDialog>
#include <DPushButton>
#include <DLabel>
#include <DWidget>
#include <DPasswordEdit>
#include <DSuggestButton>

DWIDGET_USE_NAMESPACE

class QNetworkReply;
class QNetworkAccessManager;
class ModifyDialog : public DDialog
{
    Q_OBJECT
public:
    ModifyDialog(const QString &requestPrefix, const QString &machineId, QWidget *parent = nullptr);

    void setUserName(const QString &name, const QString &scenecode);

private:
    void initConnection();
    void initUI();

    void onUpdateConfirmButtonState();

private Q_SLOTS:
    void onConfirmClicked();

    void onCheckPassword();
    void onPasswordChecked(QNetworkReply *);

private:
    DPasswordEdit *m_oldPasswordEdit;
    DPasswordEdit *m_passwordEdit ;
    DPasswordEdit *m_repeatPasswordEdit;
    QNetworkAccessManager *m_modifyPasswordManager;

    QString m_userName;
    QString m_sceneCode;
    QString m_newPasswd;
    QString m_machine_id;
    QString m_requestPrefix;

    int m_confirmIndex;
};

#endif // DModifyDialog_H
