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

#include <QDebug>
#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>

#include "modify_dialog.h"
#include "src/global_util/public_func.h"

#include "src/global_util/AesEncryptoEcbmode.h"
#include "src/global_util/supportssl.h"

#define REMOVE_USER_PSWD_SCRIPT_PATH    "/usr/share/dcmc/scene/renew_cache.bash"

ModifyDialog::ModifyDialog(const QString &requestPrefix, const QString &machineId, QWidget *parent)
    : DDialog(parent)
    , m_oldPasswordEdit(new DPasswordEdit(this))
    , m_passwordEdit(new DPasswordEdit(this))
    , m_repeatPasswordEdit(new DPasswordEdit(this))
    , m_modifyPasswordManager(new QNetworkAccessManager(this))
    , m_machine_id(machineId)
    , m_requestPrefix(requestPrefix)
    , m_confirmIndex(-1)
{
    initConnection();
    initUI();
}

void ModifyDialog::initConnection()
{
    connect(m_oldPasswordEdit, &DPasswordEdit::textChanged, this, &ModifyDialog::onUpdateConfirmButtonState);
    connect(m_passwordEdit, &DPasswordEdit::textChanged, this, &ModifyDialog::onUpdateConfirmButtonState);
    connect(m_repeatPasswordEdit, &DPasswordEdit::textChanged, this, &ModifyDialog::onUpdateConfirmButtonState);
    connect(m_repeatPasswordEdit, &DPasswordEdit::editingFinished, this, &ModifyDialog::onCheckPassword);

    connect(m_modifyPasswordManager, &QNetworkAccessManager::finished, this, &ModifyDialog::onPasswordChecked);
}

void ModifyDialog::initUI()
{
    // 窗口基础属性
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setIcon(QIcon(":/images/title.svg"));
    setWindowTitle("修改密码");
    setOnButtonClickedClose(false);
    setSpacing(10);

    // 界面布局
    QLineEdit *oldpasswordEdit = m_oldPasswordEdit->lineEdit();
    oldpasswordEdit->setPlaceholderText("请输入旧密码");
    oldpasswordEdit->setEchoMode(QLineEdit::Password);
    oldpasswordEdit->setContextMenuPolicy(Qt::NoContextMenu);
    addContent(m_oldPasswordEdit);

    QLineEdit *passwordedit = m_passwordEdit->lineEdit();
    passwordedit->setPlaceholderText("请输入新密码");
    passwordedit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setContextMenuPolicy(Qt::NoContextMenu);
    addContent(m_passwordEdit);

    QLineEdit *repasswordEdit = m_repeatPasswordEdit->lineEdit();
    repasswordEdit->setPlaceholderText("请再次输入新密码");
    repasswordEdit->setEchoMode(QLineEdit::Password);
    repasswordEdit->setContextMenuPolicy(Qt::NoContextMenu);
    addContent(m_repeatPasswordEdit);

    QAbstractButton *cancelBtn = getButton(addButton("取消"));
    connect(cancelBtn, &QAbstractButton::clicked, this, &ModifyDialog::close);

    m_confirmIndex = addButton("确认",true, DDialog::ButtonRecommend);
    QAbstractButton *confirmBtn = getButton(m_confirmIndex);
    confirmBtn->setEnabled(false);
    connect(confirmBtn, &QAbstractButton::clicked, this, &ModifyDialog::onConfirmClicked);
}

void ModifyDialog::onCheckPassword()
{
    if (m_repeatPasswordEdit->text() != m_passwordEdit->text()) {
        m_repeatPasswordEdit->setAlert(true);
        m_repeatPasswordEdit->showAlertMessage("密码不一致");
    } else {
        m_repeatPasswordEdit->setAlert(false);
        m_repeatPasswordEdit->hideAlertMessage();
    }
}

void ModifyDialog::onUpdateConfirmButtonState()
{
    if (QAbstractButton *btn = getButton(m_confirmIndex)) {
        btn->setEnabled(!this->m_oldPasswordEdit->text().isEmpty()
                        && !this->m_passwordEdit->text().isEmpty()
                        && this->m_passwordEdit->text() == this->m_repeatPasswordEdit->text());
    }
}

void ModifyDialog::onConfirmClicked()
{
    //Sha512加密小写
    QString stroldPWD = m_oldPasswordEdit->text();
    QString strPWD = m_repeatPasswordEdit->text();

    m_newPasswd = strPWD;
    QString stroldMD5PWD = QString::fromStdString(dmcg_ecb_encrypt(stroldPWD.toStdString()));
    QString strnewMD5PWD = QString::fromStdString(dmcg_ecb_encrypt(strPWD.toStdString()));

    QString url = QString("%1/api/client/modifyaccount?username=%2&password=%3&newpassword=%4&machine_id=%5&newlen=%6&scene_code=%7")
            .arg(m_requestPrefix).arg(m_userName).arg(stroldMD5PWD).arg(strnewMD5PWD).arg(m_machine_id).arg(m_repeatPasswordEdit->text().length()).arg(m_sceneCode);
    qInfo() << "request url: " << url;
    QNetworkRequest request = QNetworkRequest(QUrl(url));
    SupportSsl::instance().supportHttps(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_modifyPasswordManager->post(request, "");
}

void ModifyDialog::onPasswordChecked(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "reply status code: " << statusCode;
    QByteArray data = reply->readAll();

    auto showMessageDialog = [ = ] (const QString &message) {
        DDialog dialog(this);
        dialog.addButton("确定", false, DDialog::ButtonNormal);
        dialog.setMessage(message);
        dialog.exec();
    };

    // 数据格式错误
    QJsonParseError err;
    QJsonObject obj = QJsonDocument::fromJson(data, &err).object();
    if (err.error != QJsonParseError::NoError) {
        m_newPasswd.clear();
        qDebug() << "ERROR, failed to parse the json data from server: " << data;
        if(statusCode == 400 || statusCode == 401 || statusCode == 403 || statusCode == 423 || statusCode == 500) {
            showMessageDialog(QString("密码修改失败, 原因：%1").arg(err.errorString()));
        } else {
            showMessageDialog(QString("连接服务器失败"));
        }
        return;
    }

    // 解析服务端返回的json数据
    if (0 != obj["code"].toInt()) {
        m_newPasswd.clear();
        showMessageDialog(QString("密码修改失败, 原因：%1").arg(obj["message"].toString()));
    } else {
        // 修改成功,清除密码缓存
        QStringList args;
        args.append(REMOVE_USER_PSWD_SCRIPT_PATH);
        args.append(m_userName);
        args.append(m_newPasswd);
        runByRoot(REMOVE_USER_PSWD_SCRIPT_PATH, args);

        showMessageDialog(QString("修改成功"));
        // 关闭
        close();
    }
}

void ModifyDialog::setUserName(const QString &name, const QString &scenecode)
{
    m_userName = name;
    m_sceneCode = scenecode;
}
