// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "login_module.h"

#include <QBoxLayout>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>

#include <DConfig>

DCORE_USE_NAMESPACE

namespace dss {
namespace module {

LoginModule::LoginModule(QObject *parent)
    : QObject(parent)
    , m_callback(nullptr)
    , m_callbackFun(nullptr)
    , m_messageCallbackFunc(nullptr)
    , m_loginWidget(nullptr)
    , m_appType(AppType::Login)
    , m_loadPluginType(Notload)
    , m_isAcceptFingerprintSignal(false)
    , m_waitAcceptSignalTimer(nullptr)
    , m_dconfig(nullptr)
    , m_spinner(nullptr)
    , m_acceptSleepSignal(false)
    , m_authStatus(AuthStatus::None)
    , m_needSendAuthType(false)
    , m_isLocked(false)
    , m_login1SessionSelf(nullptr)
    , m_IdentifyWithMultipleUserStarted(false)
{
    setObjectName(QStringLiteral("LoginModule"));

    const bool isLock = qApp->applicationName().toLower().contains("lock");
    qDebug() << "Is lock application: " << isLock << ", application name: "<< qApp->applicationName();
    const QString &dconfigFile = isLock ? "org.deepin.dde.lock" : "org.deepin.dde.lightdm-deepin-greeter";
    m_dconfig = DConfig::create(dconfigFile, dconfigFile, QString(), this);
    //华为机型,从override配置中获取是否加载插件
    if (m_dconfig) {
        bool showPlugin = m_dconfig->value("enableOneKeylogin", false).toBool();
        m_loadPluginType = showPlugin ? Load : Notload;
        if (!showPlugin)
            return;
    }

    QDBusInterface login1Inter("org.freedesktop.login1", "/org/freedesktop/login1",
                                   "org.freedesktop.login1.Manager",
                                   QDBusConnection::systemBus());
    if(login1Inter.isValid()){
        QDBusPendingReply<QDBusObjectPath> reply = login1Inter.asyncCall("GetSessionByPID" , uint(0));
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, [this, reply, watcher] {
            if (!watcher->isError()) {
               QString session_self = reply.value().path();
               qDebug() << "session_self path" << session_self;
               m_login1SessionSelf = new QDBusInterface("org.freedesktop.login1", session_self, "org.freedesktop.login1.Session", QDBusConnection::systemBus());
            } else {
                qWarning() << "m_login1Inter:" << watcher->error().message();
            }
            watcher->deleteLater();
        });
    }

    initConnect();
    startCallHuaweiFingerprint();

    //在2.5秒内没接收到一键登录信号，视为失败
    m_waitAcceptSignalTimer = new QTimer(this);
    connect(m_waitAcceptSignalTimer, &QTimer::timeout, this, [this] {
        qInfo() << Q_FUNC_INFO << "start 2.5s, m_isAcceptFingerprintSignal" << m_isAcceptFingerprintSignal;
        QDBusMessage m = QDBusMessage::createMethodCall("com.deepin.daemon.Authenticate", "/com/deepin/daemon/Authenticate/Fingerprint",
                                                                                 "com.deepin.daemon.Authenticate.Fingerprint",
                                                                                 "StopIdentifyWithMultipleUser");
        // 将消息发送到Dbus
        QDBusConnection::systemBus().call(m);
        m_waitAcceptSignalTimer->stop();
        m_IdentifyWithMultipleUserStarted = false;
        if (!m_isAcceptFingerprintSignal) {
            // 防止刚切换指纹认证stop还没结束。
            QTimer::singleShot(30, this, [this] {
                sendAuthTypeToSession(AuthType::AT_Fingerprint);
            });
        }
    }, Qt::DirectConnection);
    m_waitAcceptSignalTimer->setInterval(800);
    m_waitAcceptSignalTimer->start();
}

LoginModule::~LoginModule()
{
    if (m_loginWidget) {
        delete m_loginWidget;
    }

    if (m_login1SessionSelf) {
        delete m_login1SessionSelf;
        m_login1SessionSelf = nullptr;
    }

    if (m_waitAcceptSignalTimer) {
        m_waitAcceptSignalTimer->stop();
        delete m_waitAcceptSignalTimer;
    }

    if (m_dconfig) {
        delete m_dconfig;
    }
}

void LoginModule::initConnect()
{
    bool isConIsSleep = QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1","org.freedesktop.login1.Manager",
                                                    "PrepareForSleep", this, SLOT(slotPrepareForSleep(bool)));
    qInfo() << Q_FUNC_INFO << "connect SlotPrepareForSleep: " << isConIsSleep;


    bool isConnectSuccess = QDBusConnection::systemBus().connect("com.deepin.daemon.Authenticate", "/com/deepin/daemon/Authenticate/Fingerprint","com.deepin.daemon.Authenticate.Fingerprint",
                                                    "VerifyStatus", this, SLOT(slotIdentifyStatus(const QString &, const int, const QString &)));
    qInfo() << Q_FUNC_INFO << "isconnectsuccess: " << isConnectSuccess;

}

void LoginModule::startCallHuaweiFingerprint()
{
    QDBusMessage m = QDBusMessage::createMethodCall("com.deepin.daemon.Authenticate", "/com/deepin/daemon/Authenticate/Fingerprint",
                                                                             "com.deepin.daemon.Authenticate.Fingerprint",
                                                                             "IdentifyWithMultipleUser");
    // 将消息发送到Dbus
    QDBusPendingCall identifyCall = QDBusConnection::systemBus().asyncCall(m);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(identifyCall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, identifyCall, watcher] {
        qDebug() << Q_FUNC_INFO << "Get license state:" << identifyCall.error().message();
        if (!identifyCall.isError()) {
            QDBusMessage response = identifyCall.reply();
            //判断Method是否被正确返回
            if (response.type()== QDBusMessage::ReplyMessage) {
                m_IdentifyWithMultipleUserStarted = true;
                qDebug() << Q_FUNC_INFO << "dbus IdentifyWithMultipleUser call success";
            } else {
                qWarning() << Q_FUNC_INFO << "dbus IdentifyWithMultipleUser call failed";
                m_isAcceptFingerprintSignal = false;
                //FIXME 此处不能调用回调，因为还没初始化，此处的逻辑应该在setCallBack函数完成后再进行。
                sendAuthTypeToSession(AuthType::AT_Fingerprint);
            }
        }
        watcher->deleteLater();
    });
}

void LoginModule::init()
{
    initUI();
    updateInfo();

    if (m_appType == AppType::Lock && !m_acceptSleepSignal) {
        //此处延迟0.5s发送，是需要等待dde-session-shell认证方式创建完成
        QTimer::singleShot(500, this, [this] {
            sendAuthTypeToSession(AuthType::AT_Fingerprint);
        });
    }
}

void LoginModule::reset()
{
    init();
}

void LoginModule::initUI()
{
    qInfo() << Q_FUNC_INFO;
    if (m_loginWidget) {
        qInfo() << Q_FUNC_INFO << "m_loginWidget is exist";
        return;
    }
    m_loginWidget = new QWidget;
    m_loginWidget->setAccessibleName(QStringLiteral("LoginWidget"));
    m_loginWidget->setMinimumSize(260, 100);

    m_loginWidget->setLayout(new QHBoxLayout);
    m_spinner = new DTK_WIDGET_NAMESPACE::DSpinner(m_loginWidget);
    m_spinner->setFixedSize(40, 40);
    m_loginWidget->layout()->addWidget(m_spinner);
    m_spinner->start();

}

void LoginModule::updateInfo()
{
    qInfo() << Q_FUNC_INFO;
    if (!m_messageCallbackFunc) {
        qWarning() << Q_FUNC_INFO << "message callback func is nullptr";
        return;
    }

    // 发送获取属性的请求
    QJsonObject message;
    message.insert("CmdType", "GetProperties");
    QJsonArray array;
    array.append("AppType");
    array.append("CurrentUser");
    message["Data"] = array;
    QJsonDocument doc;
    doc.setObject(message);
    std::string ret = m_messageCallbackFunc(doc.toJson().toStdString(), m_callback->app_data);

    // 解析返回值
    QJsonParseError jsonParseError;
    const QJsonDocument retDoc = QJsonDocument::fromJson(QByteArray::fromStdString(ret), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || retDoc.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Failed to analysis AppType info from shell!: " << QString::fromStdString(ret);
        return ;
    }

    QJsonObject obj = retDoc.object();
    if (obj.value("Code").toInt() != 0) {
        qWarning() << "Get properties failed, message: " << obj.value("Message").toString();
        return;
    }

    QJsonObject data = obj.value("Data").toObject();
    if (data.contains("AppType")) {
        m_appType = (AppType)data.value("AppType").toInt();
        qInfo() << Q_FUNC_INFO << "App type: " << m_appType;
    }

    if (data.contains("CurrentUser")) {
        QJsonObject user = data.value("CurrentUser").toObject();
        m_userName = user.value("Name").toString();
        qInfo() << Q_FUNC_INFO << "Current user: " << user;
    }
}

void LoginModule::setCallback(LoginCallBack *callback)
{
    qInfo() << Q_FUNC_INFO;
    m_callback = callback;
    m_callbackFun = callback->authCallbackFun;
    m_messageCallbackFunc = callback->messageCallbackFunc;
}

std::string LoginModule::onMessage(const std::string &message)
{
    qInfo() << Q_FUNC_INFO;
    QJsonParseError jsonParseError;
    const QJsonDocument messageDoc = QJsonDocument::fromJson(QByteArray::fromStdString(message), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || messageDoc.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Failed to obtain message from shell!: " << QString::fromStdString(message);
        return "";
    }

    QJsonObject retObj;
    retObj["Code"] = 0;
    retObj["Message"] = "Success";

    QJsonObject msgObj = messageDoc.object();
    QString cmdType = msgObj.value("CmdType").toString();
    QJsonObject data = msgObj.value("Data").toObject();
    qInfo() << "Cmd type: " << cmdType;
    if (cmdType == "CurrentUserChanged") {
        m_userName = data.value("Name").toString();
        qDebug() << "Current user changed, user name: " << m_userName;
    } else if (cmdType == "GetConfigs") {
        QJsonObject retDataObj;
        retDataObj["ShowAvatar"] = false;
        retDataObj["ShowUserName"] = false;
        retDataObj["ShowSwitchButton"] = false;
        retDataObj["ShowLockButton"] = false;
        retDataObj["DefaultAuthLevel"] = DefaultAuthLevel::StrongDefault;
        retDataObj["AuthType"] = AuthType::AT_Custom;

        retObj["Data"] = retDataObj;
    } else if (cmdType == "StartAuth"){
        qDebug() << Q_FUNC_INFO << "startAuth" << m_lastAuthResult.result << QString::fromStdString(m_lastAuthResult.account);
        m_authStatus = AuthStatus::Start;
        int type = data.value("AuthObjectType").toInt();
        if(type == AuthObjectType::LightDM ){
            if (m_isAcceptFingerprintSignal){
                sendAuthData(m_lastAuthResult);
            }
            if (m_needSendAuthType) {
                sendAuthTypeToSession(AuthType::AT_Fingerprint);
            }
        }
    } else if (cmdType == "AuthState") {
        int authType = data.value("AuthType").toInt();
        int authState = data.value("AuthState").toInt();
        // 所有类型验证成功，重置插件状态
        if (authType == AuthType::AT_All && authState == AuthState::AS_Success) {
            init();
        }
    } else if (cmdType == "LimitsInfo") {
        QString info = data.value("LimitsInfoStr").toString();
        qDebug() << Q_FUNC_INFO << "LimitsInfoStr" << info;
        const QJsonDocument limitsInfoDoc = QJsonDocument::fromJson(info.toUtf8());
        const QJsonArray limitsInfoArr = limitsInfoDoc.array();
        for (const QJsonValue &limitsInfoStr : limitsInfoArr) {
            const QJsonObject limitsInfoObj = limitsInfoStr.toObject();
            if (limitsInfoObj["flag"].toInt() == AT_Password)
                m_isLocked = limitsInfoObj["locked"].toBool();
        }

    }

    QJsonDocument doc;
    doc.setObject(retObj);
    return doc.toJson().toStdString();
}

void LoginModule::slotIdentifyStatus(const QString &name, const int errorCode, const QString &msg)
{
    qDebug() << Q_FUNC_INFO << "LoginModule name :" << name << "\n error code:" << errorCode << " \n error msg:" << msg;
    m_waitAcceptSignalTimer->stop();
    if(m_IdentifyWithMultipleUserStarted){
        QDBusMessage m = QDBusMessage::createMethodCall("com.deepin.daemon.Authenticate", "/com/deepin/daemon/Authenticate/Fingerprint",
                                                                                 "com.deepin.daemon.Authenticate.Fingerprint",
                                                                                 "StopIdentifyWithMultipleUser");
        // 将消息发送到Dbus
        QDBusConnection::systemBus().call(m);
    }

    m_IdentifyWithMultipleUserStarted = false;
    m_isAcceptFingerprintSignal = true;
    m_lastAuthResult = AuthCallbackData();
    if (errorCode == 0) {
        m_acceptSleepSignal = false;

        if (m_appType == AppType::Lock && m_userName != name && name != "") {
            // 防止stop还未完成就开启了指纹认证
            QTimer::singleShot(30, this, [this] {
                sendAuthTypeToSession(AuthType::AT_Fingerprint);
            });
            return ;
        }

        qInfo() << Q_FUNC_INFO << "singleShot verify";
        m_lastAuthResult.account = name == "" ? m_userName.toStdString() : name.toStdString();
        m_lastAuthResult.result = AuthResult::Success;
        if(m_authStatus == AuthStatus::Start || m_appType == AppType::Lock){
            sendAuthData(m_lastAuthResult);
        }
    } else {
        // 发送一键登录失败的信息
        qWarning() << Q_FUNC_INFO << "slotIdentifyStatus recive failed";
        QTimer::singleShot(30, this, [this] {
            sendAuthTypeToSession(AuthType::AT_Fingerprint);
        });

        m_lastAuthResult.result = AuthResult::Failure;
        m_lastAuthResult.message = QString::number(errorCode).toStdString();
        m_lastAuthResult.account = name.toStdString();
        sendAuthData(m_lastAuthResult);
    }
}

void LoginModule::sendAuthData(AuthCallbackData& data)
{
    if (!m_callbackFun) {
        qDebug() << Q_FUNC_INFO << "m_callbackFun is null";
        return;
    }

    if (m_spinner) {
        m_spinner->stop();
    }
    m_callbackFun(&data, m_callback->app_data);
    m_authStatus = AuthStatus::Finish;
}

void LoginModule::slotPrepareForSleep(bool active)
{
    qInfo() << Q_FUNC_INFO << active;

    m_acceptSleepSignal = true;
    if (m_login1SessionSelf == nullptr) {
        qWarning()  << "m_login1SessionSelf is null";
        return;
    }

    if (!m_login1SessionSelf->isValid()) {
        qWarning()  << "m_login1SessionSelf is not Valid";
        return;
    }

    bool isSessionAvtive = m_login1SessionSelf->property("Active").toBool();

    m_lastAuthResult = AuthCallbackData();
    // 休眠待机时,同时当前session被激活才开始调用一键登录指纹
    if (active) {
        m_lastAuthResult.result = AuthResult::Failure;
        sendAuthData(m_lastAuthResult);
        sendAuthTypeToSession(AuthType::AT_Custom);
        return;
    }

    if (isSessionAvtive) {
        m_isAcceptFingerprintSignal = false;
        sendAuthTypeToSession(AuthType::AT_Custom);
        // 等待切换到插件认证完成后再发起多用户认证
        QTimer::singleShot(300, this, [this] {
            startCallHuaweiFingerprint();
        });
        if (m_spinner)
            m_spinner->start();
        // s3/s4机器认证结果返回可能比较久,所以这里设置等待结果的时间为最大2500ms保证结果一定返回了
        m_waitAcceptSignalTimer->setInterval(2500);
        m_waitAcceptSignalTimer->start();
    } else {
        //fix: 多用户时，第一个用户直接锁屏，然后待机唤醒，在直接切换到另一个用户时，m_login1SessionSelf没有激活，见159949
        sendAuthTypeToSession(AT_Fingerprint);
    }
}

void LoginModule::sendAuthTypeToSession(AuthType type)
{
    qInfo() << Q_FUNC_INFO << "sendAuthTypeToSession" << type;
    if (!m_messageCallbackFunc){
        m_needSendAuthType = true;
        return;
    }
    // 这里主要为了防止 在发送切换信号的时候,lightdm还为开启认证，导致切换类型失败
    if (m_authStatus == AuthStatus::None && !m_isLocked && type != AuthType::AT_Custom && m_appType != AppType::Lock) {
        m_needSendAuthType = true;
        return;
    }
    if (m_spinner && type != AuthType::AT_Custom) {
        m_acceptSleepSignal = false;
        m_spinner->stop();
    }
    QJsonObject message;
    message.insert("CmdType", "setAuthTypeInfo");
    QJsonObject retDataObj;
    retDataObj["AuthType"] = type;
    message["Data"] = retDataObj;
    QJsonDocument doc;
    doc.setObject(message);
    std::string ret = m_messageCallbackFunc(doc.toJson().toStdString(), m_callback->app_data);
    // 解析返回值
    QJsonParseError jsonParseError;
    const QJsonDocument retDoc = QJsonDocument::fromJson(QByteArray::fromStdString(ret), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || retDoc.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Failed to analysis SlotPrepareForSleep info from shell!: " << QString::fromStdString(ret);
    }
    m_needSendAuthType = false;
}
} // namespace module
} // namespace dss
