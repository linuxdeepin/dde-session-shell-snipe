// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gestureauthworker.h"
#include "waypointmodel.h"
#include "login_module_interface_v2.h"
#include "translastiondoc.h"
#include "tokenCrypt.h"

#include <QDateTime>

using namespace gestureLogin;

// 负责手势认证过程
GestureAuthWorker::GestureAuthWorker(QObject *parent)
    : QObject(parent)
    , m_userService(nullptr)
    , m_stateMachine(nullptr)

{
    m_unLockTimer.setSingleShot(true);
    m_remaindTimer.setSingleShot(false);
    m_remaindTimer.setInterval(1000 * 60);

    initConnection();
}

void GestureAuthWorker::setLocalPassword(const QString &localPassword)
{
    m_localPasswd = localPassword;
}

bool GestureAuthWorker::gestureEnabled()
{
    return true;
}

bool GestureAuthWorker::gestureExist()
{
    return true;
}

void GestureAuthWorker::setActive(bool active)
{
    if (active) {
        initState();
    } else {
        if (m_stateMachine) {
            m_stateMachine->stop();
            m_stateMachine->deleteLater();
            m_stateMachine = nullptr;
        }
    }
}

void GestureAuthWorker::initConnection()
{
    WayPointModel *model = WayPointModel::instance();
    connect(model, &WayPointModel::gestureStateChanged, this, [this, model](int state) {
        if (model->appType() == ModelAppType::LoginLock) {
            bool enrolled = state == GestureState::Set;
            setActive(enrolled);
            model->setCurrentMode(enrolled ? Mode::Auth : Mode::Enroll);
        }
    });

    // 只处理认证过程
    connect(model, &WayPointModel::pathDone, this, [this, model] {
        // 仅处理认证过程
        auto mode = model->currentMode();
        auto type = model->appType();
        if (mode == Mode::Auth) {
            // 在登录锁屏上解锁
            if (type == ModelAppType::LoginLock) {
                qDebug() << "worker send token";
                Q_EMIT requestSendToken(model->getToken());
            }

            // 重置密码对话框上的认证是一个单纯的本地密文对比的过程
            if (type == ModelAppType::Reset) {
                QString token = gestureEncrypt::cryptUserPassword(model->getToken(), m_localPasswd.toUtf8());
                // 从外部获取的密文与本地对比
                if (token == m_localPasswd) {
                    Q_EMIT model->authDone();
                } else {
                    Q_EMIT model->authError();
                }
            }
        }
    });
}

void GestureAuthWorker::initState()
{
    if (m_stateMachine) {
        delete m_stateMachine;
        m_stateMachine = nullptr;
    }

    m_stateMachine = new QStateMachine(m_stateMachine);

    auto model = WayPointModel::instance();
    auto tanslator = TranslastionDoc::instance();

    QString startTitle = tanslator->getDoc(model->appType() == ModelAppType::Reset ? DocIndex::ModifyPasswd : DocIndex::LoginStart);

    // 开始认证
    auto startState = new QState(m_stateMachine);
    startState->assignProperty(model, "title", startTitle);
    startState->assignProperty(model, "tip", tanslator->getDoc(model->appType() == ModelAppType::Reset ? DocIndex::DrawCurrentPasswd : DocIndex::RequestDrawing));
    // 认证失败，需要更新锁定时间与错误次数
    auto authError = new QState(m_stateMachine);
    auto authErrTip = model->errorTextStyle().arg(tanslator->getDoc(DocIndex::AuthErrorWithTimes));
    switch (model->appType()) {
    case ModelAppType::LoginLock:
        // authError->assignProperty(model, "title", tanslator->getDoc(DocIndex::LoginStart));
        // authError->assignProperty(model, "tip", authErrTip);
        break;
    case ModelAppType::Reset:
        authError->assignProperty(model, "title", tanslator->getDoc(DocIndex::ModifyPasswd));
        authError->assignProperty(model, "tip", tanslator->getDoc(DocIndex::ContactAdmin));
        break;
    default:
        break;
    }

    // 输入错误提示
    auto inputError = new QState(m_stateMachine);
    inputError->assignProperty(model, "title", startTitle);
    auto pathErrorTip = model->errorTextStyle().arg(tanslator->getDoc(DocIndex::PathError));
    inputError->assignProperty(model, "tip", pathErrorTip);

    startState->addTransition(model, SIGNAL(pathError()), inputError);
    startState->addTransition(model, SIGNAL(authError()), authError);
    startState->addTransition(model, SIGNAL(authDone()), startState);

    inputError->addTransition(model, SIGNAL(authDone()), startState);
    inputError->addTransition(model, SIGNAL(authError()), authError);
    inputError->addTransition(model, SIGNAL(pathError()), inputError);

    authError->addTransition(model, SIGNAL(pathError()), inputError);
    authError->addTransition(model, SIGNAL(authDone()), startState);
    authError->addTransition(model, SIGNAL(authError()), authError);

    m_stateMachine->setInitialState(startState);
    m_stateMachine->start();
}

void GestureAuthWorker::onAuthStateChanged(int authType, int authState)
{
    using dss::module::AuthType;
    using dss::module::AuthState;

    if (WayPointModel::instance()->currentMode() != Mode::Auth) {
        return;
    }

    if (authType == AuthType::AT_All && authState == AuthState::AS_Success) {
        Q_EMIT WayPointModel::instance()->authDone();
    }

    if (authType != AuthType::AT_Pattern) {
        return;
    }

    switch (authState) {
    case AuthState::AS_Success:
        Q_EMIT WayPointModel::instance()->authDone();
        break;
    case AuthState::AS_Failure:
        Q_EMIT WayPointModel::instance()->authError();
        break;
    case AuthState::AS_Locked:
        // 认证发送的lock状态，代表当前认证锁定
        WayPointModel::instance()->setLocked(true);
        break;
    case AuthState::AS_Unlocked:
        // 认证发送的unlock,代表当前认证解锁
        WayPointModel::instance()->setLocked(false);
        break;
    case AuthState::AS_Started:
        // 插件框架响应的计时器重置认证,代表当前认证解锁
        WayPointModel::instance()->setLocked(false);
        break;
    default:
        break;
    }
}

// 这里只处理UI响应，model的状态设置交给authStatus控制
void GestureAuthWorker::onLimitsInfoChanged(bool locked, int maxTries, int numFailures, int unlockSecs, const QString &unlockTime)
{
    auto model = WayPointModel::instance();
    if (model->currentMode() != Mode::Auth) {
        return;
    }

    do {
        auto translator = TranslastionDoc::instance();
        // 未锁定状态
        if (numFailures && maxTries && numFailures < maxTries && !unlockSecs) {
            uint remains = static_cast<uint>(maxTries - numFailures);
            auto remainStr = model->errorTextStyle().arg(translator->getDoc(DocIndex::AuthErrorWithTimes).arg(remains));
            model->setProperty("tip", remainStr);
            break;
        }

        m_unLockTimer.stop();
        m_unLockTimer.disconnect();

        m_remaindTimer.stop();
        m_remaindTimer.disconnect();

        model->setProperty("title", TranslastionDoc::instance()->getDoc(DocIndex::LoginStart));
        model->setProperty("tip", TranslastionDoc::instance()->getDoc(DocIndex::RequestDrawing));

        // unlockSecs 是唯一判断条件
        if (unlockSecs) {
            qDebug() << "input should be locked from limit info content";
            // 显示锁定时间
            uint intervalSeconds = QDateTime::fromString(unlockTime, Qt::ISODateWithMs).toLocalTime().toTime_t()
                                   - QDateTime::currentDateTimeUtc().toTime_t();
            // 每次设置时，强制更新计时器
            m_unLockTimer.setInterval(static_cast<int>(intervalSeconds) * 1000);
            connect(&m_unLockTimer, &QTimer::timeout, this, [this] {
                m_remaindTimer.stop();
                // 信号通知解除界面禁用
                auto model = WayPointModel::instance();
                model->setProperty("title", TranslastionDoc::instance()->getDoc(DocIndex::LoginStart));
                model->setProperty("tip", TranslastionDoc::instance()->getDoc(DocIndex::RequestDrawing));
            });

            m_lockMinutes = unlockSecs / 60;
            if (unlockSecs % 60) {
                m_lockMinutes += 1;
            }

            auto lockString = translator->getDoc(DocIndex::DeviceLock).arg(m_lockMinutes);
            model->setProperty("title", lockString);
            model->setProperty("tip", model->errorTextStyle().arg(translator->getDoc(DocIndex::DeviceLockHelp)));

            if (m_lockMinutes) {
                connect(&m_remaindTimer, &QTimer::timeout, this, [this] {
                    // 更新锁定时间
                    m_lockMinutes--;
                    if (m_lockMinutes) {
                        auto errStr = TranslastionDoc::instance()->getDoc(DocIndex::DeviceLock).arg(m_lockMinutes);
                        WayPointModel::instance()->setProperty("title", errStr);
                    }
                });
            }

            m_remaindTimer.start();
            m_unLockTimer.start();

            break;
        }
    } while (false);
}
