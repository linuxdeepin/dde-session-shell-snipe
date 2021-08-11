/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zorowk <near.kingzero@gmail.com>
 *
 * Maintainer: zorowk <near.kingzero@gmail.com>
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

#include "userexpiredwidget.h"
#include "framedatabind.h"
#include "userinfo.h"
#include "src/widgets/loginbutton.h"
#include "src/global_util/constants.h"
#include "src/session-widgets/framedatabind.h"
#include "src/global_util/public_func.h"
#include "src/global_util/AesEncryptoEcbmode.h"
#include "src/global_util/supportssl.h"
#include "src/widgets/dpasswordeditex.h"

#include <DFontSizeManager>
#include <DPalette>
#include "dhidpihelper.h"

#include <QNetworkReply>
#include <QVBoxLayout>
#include <QPushButton>
#include <QAction>
#include <QImage>

#define REMOVE_USER_PSWD_SCRIPT_PATH    "/usr/share/dcmc/scene/renew_cache.bash"
static const int BlurRectRadius = 15;
static const int WidgetsSpacing = 10;

UserExpiredWidget::UserExpiredWidget(QWidget *parent)
    : QWidget(parent)
    , m_blurEffectWidget(new DBlurEffectWidget(this))
    , m_nameLbl(new QLabel(this))
    , m_expiredTips(new DLabel(this))
    , m_passwordEdit(new DPasswordEditEx(this))
    , m_confirmPasswordEdit(new DPasswordEditEx(this))
    , m_lockButton(new DFloatingButton(DStyle::SP_ArrowNext))
    , m_modifyPasswordManager(new QNetworkAccessManager(this))
{
    initUI();
    initConnect();
}

UserExpiredWidget::~UserExpiredWidget()
{

}

//重置控件的状态
void UserExpiredWidget::resetAllState()
{
    m_passwordEdit->lineEdit()->clear();
    m_confirmPasswordEdit->lineEdit()->clear();
    if (m_authType == SessionBaseModel::LightdmType) {
        m_lockButton->setIcon(DStyle::SP_ArrowNext);
    } else {
        m_lockButton->setIcon(DStyle::SP_UnlockElement);
    }
}

void UserExpiredWidget::setFaildTipMessage(const QString &message)
{
    if (message.isEmpty()) {
        m_passwordEdit->hideAlertMessage();
    } else if (m_passwordEdit->isVisible()) {
        m_passwordEdit->showAlertMessage(message, -1);
        m_passwordEdit->lineEdit()->selectAll();
    }
}

void UserExpiredWidget::onOtherPageConfirmPasswordChanged(const QVariant &value)
{
    int cursorIndex =  m_confirmPasswordEdit->lineEdit()->cursorPosition();
    m_confirmPasswordEdit->setText(value.toString());
    m_confirmPasswordEdit->lineEdit()->setCursorPosition(cursorIndex);
}

void UserExpiredWidget::onOtherPagePasswordChanged(const QVariant &value)
{
    int cursorIndex =  m_passwordEdit->lineEdit()->cursorPosition();
    m_passwordEdit->setText(value.toString());
    m_passwordEdit->lineEdit()->setCursorPosition(cursorIndex);
}

void UserExpiredWidget::refreshBlurEffectPosition()
{
    QRect rect = m_userLayout->geometry();
    rect.setTop(rect.top() + m_userLayout->margin());

    m_blurEffectWidget->setGeometry(rect);
}

//窗体resize事件,更新阴影窗体的位置
void UserExpiredWidget::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, &UserExpiredWidget::refreshBlurEffectPosition);

    return QWidget::resizeEvent(event);
}

void UserExpiredWidget::showEvent(QShowEvent *event)
{
    refreshBlurEffectPosition();
    updateNameLabel();
    return QWidget::showEvent(event);
}

void UserExpiredWidget::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event);

    m_passwordEdit->hideAlertMessage();
    m_confirmPasswordEdit->hideAlertMessage();
}

bool UserExpiredWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
            if ((key_event->modifiers() & Qt::ControlModifier) && key_event->key() == Qt::Key_A) return false;
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

//初始化窗体控件
void UserExpiredWidget::initUI()
{
    QPalette palette = m_nameLbl->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    m_nameLbl->setPalette(palette);
    m_nameLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    DFontSizeManager::instance()->bind(m_nameLbl, DFontSizeManager::T2);
    m_nameLbl->setAlignment(Qt::AlignCenter);

    m_expiredTips->setText(tr("Password expired, please change"));
    m_expiredTips->setAlignment(Qt::AlignCenter);

    setFocusProxy(m_passwordEdit->lineEdit());
    m_passwordEdit->lineEdit()->setPlaceholderText(tr("New password"));
    m_passwordEdit->lineEdit()->setContextMenuPolicy(Qt::NoContextMenu);
    m_passwordEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_passwordEdit->setFixedHeight(DDESESSIONCC::PASSWDLINEEDIT_HEIGHT);
    m_passwordEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_passwordEdit->setFocusPolicy(Qt::StrongFocus);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setClearButtonEnabled(false);
    m_passwordEdit->lineEdit()->installEventFilter(this);

    m_confirmPasswordEdit->lineEdit()->setPlaceholderText(tr("Repeat password"));
    m_confirmPasswordEdit->lineEdit()->setContextMenuPolicy(Qt::NoContextMenu);
    m_confirmPasswordEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_confirmPasswordEdit->setFixedHeight(DDESESSIONCC::PASSWDLINEEDIT_HEIGHT);
    m_confirmPasswordEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    m_confirmPasswordEdit->setFocusPolicy(Qt::StrongFocus);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setClearButtonEnabled(false);
    m_confirmPasswordEdit->lineEdit()->installEventFilter(this);

    m_userLayout = new QVBoxLayout;
    m_userLayout->setMargin(WidgetsSpacing);
    m_userLayout->setSpacing(WidgetsSpacing);

    m_nameLayout = new QHBoxLayout;
    m_nameLayout->setMargin(10);
    m_nameLayout->setSpacing(5);
    m_nameLayout->addWidget(m_nameLbl);
    m_nameFrame = new QFrame;
    m_nameFrame->setLayout(m_nameLayout);
    m_userLayout->addWidget(m_nameFrame, 0, Qt::AlignHCenter);

    m_userLayout->addWidget(m_expiredTips);
    m_userLayout->addWidget(m_passwordEdit);
    m_userLayout->addWidget(m_confirmPasswordEdit);

    m_blurEffectWidget->setMaskColor(DBlurEffectWidget::LightColor);
    m_blurEffectWidget->setMaskAlpha(76);
    m_blurEffectWidget->setBlurRectXRadius(BlurRectRadius);
    m_blurEffectWidget->setBlurRectYRadius(BlurRectRadius);
    m_lockButton->setFocusPolicy(Qt::StrongFocus);

    m_lockLayout = new QVBoxLayout;
    m_lockLayout->setMargin(0);
    m_lockLayout->setSpacing(0);
    m_lockLayout->addWidget(m_lockButton, 0, Qt::AlignHCenter);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setMargin(0);
    mainLayout->addStretch();
    mainLayout->addLayout(m_userLayout);
    mainLayout->addLayout(m_lockLayout);
    //此处插入18的间隔，是为了在登录和锁屏多用户切换时，绘制选中的样式（一个92*4的圆角矩形，距离阴影下边间隔14像素）
    mainLayout->addSpacing(18);
    mainLayout->addStretch();

    setLayout(mainLayout);

    setTabOrder(m_passwordEdit->lineEdit(), m_confirmPasswordEdit->lineEdit());
    setTabOrder(m_confirmPasswordEdit->lineEdit(), m_lockButton);

    QFile file("/etc/dmcg/machine.txt");
    if (!file.exists()) {
       qWarning() << "ERROR, file not exist: " << file.fileName();
       return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ERROR, file open failed: " << file.fileName();
        return;
    }

    m_machine_id = QString::fromStdString(file.readAll().toStdString());
    file.close();

    file.setFileName("/etc/dmcg/config.json");
    if (!file.exists()) {
       qWarning() << "ERROR, file not exist: " << file.fileName();
       return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ERROR, file open failed: " << file.fileName();
        return;
    }

    QJsonParseError err;
    QJsonObject obj = QJsonDocument::fromJson(file.readAll(), &err).object();
    file.close();
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "ERROR, parse json data failed, error: " << err.errorString();
        return;
    }

    QString ip = obj["server_host"].toString();
    QString port = QString::number(obj["server_port"].toInt());
    QString protocol = (port == "443") ? "https:" : "http:";

    if (ip.isEmpty() || port.isEmpty() || protocol.isEmpty() || m_machine_id.isEmpty()) {
        qWarning() << "ERROR, parameters is empty";
        return;
    }

    m_requestPrefix = QString("%1//%2:%3").arg(protocol, ip, port);
}

//初始化槽函数连接
void UserExpiredWidget::initConnect()
{
    connect(m_passwordEdit->lineEdit(), &QLineEdit::textChanged, this, [ = ](const QString & value) {
        FrameDataBind::Instance()->updateValue("UserPassword", value);
    });

    connect(m_confirmPasswordEdit->lineEdit(), &QLineEdit::textChanged, this, [ = ](const QString & value) {
        FrameDataBind::Instance()->updateValue("UserConfimPassword", value);
    });

    connect(m_passwordEdit, &DLineEdit::returnPressed, this, &UserExpiredWidget::onConfirmClicked);
    connect(m_confirmPasswordEdit, &DLineEdit::returnPressed, this, &UserExpiredWidget::onConfirmClicked);
    connect(m_lockButton, &QPushButton::clicked, this,  &UserExpiredWidget::onConfirmClicked);
    connect(m_modifyPasswordManager, &QNetworkAccessManager::finished, this, &UserExpiredWidget::onPasswordChecked);

    QMap<QString, int> registerFunctionIndexs;
    std::function<void (QVariant)> confirmPaswordChanged = std::bind(&UserExpiredWidget::onOtherPageConfirmPasswordChanged, this, std::placeholders::_1);
    registerFunctionIndexs["UserConfimPassword"] = FrameDataBind::Instance()->registerFunction("UserConfimPassword", confirmPaswordChanged);
    std::function<void (QVariant)> passwordChanged = std::bind(&UserExpiredWidget::onOtherPagePasswordChanged, this, std::placeholders::_1);
    registerFunctionIndexs["UserPassword"] = FrameDataBind::Instance()->registerFunction("UserPassword", passwordChanged);
    connect(this, &UserExpiredWidget::destroyed, this, [ = ] {
        for (auto it = registerFunctionIndexs.constBegin(); it != registerFunctionIndexs.constEnd(); ++it)
        {
            FrameDataBind::Instance()->unRegisterFunction(it.key(), it.value());
        }
    });

    QTimer::singleShot(0, this, [ = ] {
        FrameDataBind::Instance()->refreshData("UserPassword");
        FrameDataBind::Instance()->refreshData("UserConfimPassword");
    });
}

void UserExpiredWidget::setDisplayName(const QString &name)
{
    m_showName = name;
    m_nameLbl->setText(name);
}

void UserExpiredWidget::setUserName(const QString &name)
{
    m_userName = name;
}

void UserExpiredWidget::setPassword(const QString &passwd)
{
    m_password = passwd;
}

void UserExpiredWidget::updateNameLabel()
{
    int width = m_nameLbl->fontMetrics().width(m_showName);
    int labelMaxWidth = this->width() - 3 * m_nameLayout->spacing();

    if (width > labelMaxWidth) {
        QString str = m_nameLbl->fontMetrics().elidedText(m_showName, Qt::ElideRight, labelMaxWidth);
        m_nameLbl->setText(str);
    }
}

void UserExpiredWidget::updateAuthType(SessionBaseModel::AuthType type)
{
    m_authType = type;
    if (m_authType == SessionBaseModel::LightdmType) {
        m_lockButton->setIcon(DStyle::SP_ArrowNext);
    }
}

void UserExpiredWidget::onConfirmClicked()
{
    //Sha512加密小写
    QString stroldPWD = m_passwordEdit->text();
    QString strPWD = m_confirmPasswordEdit->text();

    if(stroldPWD != strPWD) {
        m_confirmPasswordEdit->showAlertMessage(QString("密码不一致"));
        return;
    }

    m_newPasswd = strPWD;
    QString stroldMD5PWD = QString::fromStdString(dmcg_ecb_encrypt(m_password.toStdString()));
    QString strnewMD5PWD = QString::fromStdString(dmcg_ecb_encrypt(strPWD.toStdString()));

    QString url = QString("%1/api/client/modifyaccount?username=%2&password=%3&newpassword=%4&machine_id=%5&newlen=%6&scene_code=intranet-scene")
            .arg(m_requestPrefix).arg(m_userName).arg(stroldMD5PWD).arg(strnewMD5PWD).arg(m_machine_id).arg(m_confirmPasswordEdit->text().length());
    qInfo() << "request url: " << url;
    QNetworkRequest request = QNetworkRequest(QUrl(url));
    SupportSsl::instance().supportHttps(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_modifyPasswordManager->post(request, "");
}


void UserExpiredWidget::onPasswordChecked(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "reply status code: " << statusCode;
    QByteArray data = reply->readAll();

    // 数据格式错误
    QJsonParseError err;
    QJsonObject obj = QJsonDocument::fromJson(data, &err).object();
    if (err.error != QJsonParseError::NoError) {
        m_newPasswd.clear();
        qDebug() << "ERROR, failed to parse the json data from server: " << data;
        if(statusCode == 400 || statusCode == 401 || statusCode == 403 || statusCode == 423 || statusCode == 500) {
            m_confirmPasswordEdit->showAlertMessage(QString("密码修改失败, 原因：%1").arg(err.errorString()));
        } else {
            m_confirmPasswordEdit->showAlertMessage(QString("连接服务器失败"));
        }
        return;
    }

    // 解析服务端返回的json数据
    if (0 != obj["code"].toInt()) {
        m_newPasswd.clear();
        m_confirmPasswordEdit->showAlertMessage(QString("密码修改失败, 原因：%1").arg(obj["message"].toString()));
    } else {
        // 修改成功,清除密码缓存
        QStringList args;
        args.append(REMOVE_USER_PSWD_SCRIPT_PATH);
        args.append(m_userName);
        args.append(m_newPasswd);
        runByRoot(REMOVE_USER_PSWD_SCRIPT_PATH, args);

        m_confirmPasswordEdit->showAlertMessage(QString("修改成功"));
        // 关闭
        close();

        emit changePasswordFinished();
    }
}
