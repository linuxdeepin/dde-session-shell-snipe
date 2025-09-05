// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gesturemodifyworker.h"
#include "waypointmodel.h"
#include "gestureauthworker.h"
#include "translastiondoc.h"

using namespace gestureSetting;
using namespace gestureLogin;

GestureModifyWorker::GestureModifyWorker(QObject *parent)
    : QObject(parent)
{
    initConnect();
}

void GestureModifyWorker::setActive(bool active)
{
    // 无论进入还是离开这个状态都清空内部数据
    m_token = "";

    auto *wayPointModel = WayPointModel::instance();
    auto connectType = Qt::AutoConnection | Qt::UniqueConnection;
    if (active) {
        connect(wayPointModel, &WayPointModel::pathError, this, &GestureModifyWorker::onPathError, Qt::ConnectionType(connectType));
        connect(wayPointModel, &WayPointModel::pathDone, this, &GestureModifyWorker::onPathDone, Qt::ConnectionType(connectType));
        wayPointModel->setProperty("title", TranslastionDoc::instance()->getDoc(DocIndex::SetPasswd));
        wayPointModel->setProperty("tip", TranslastionDoc::instance()->getDoc(DocIndex::RequestDrawing));
    }
}

void GestureModifyWorker::onPathDone()
{
    auto *wayPointModel = WayPointModel::instance();

    // 这里只处理录入密码的模式
    if (wayPointModel->currentMode() != Mode::Enroll)
        return;

    QString token = wayPointModel->getToken();
    // 密码为空，一般是从密码验证成功后进入到当前重设密码的过程，此时无需继续
    if (token.isEmpty())
        return;

    TranslastionDoc *transDoc = TranslastionDoc::instance();
    wayPointModel->clearPath();

    if (m_token.isEmpty()) {
        m_token = token;
        // 如果存储的token为空，说明是第一次设置密码成功，此时更改提示为（请重新录入手势密码）
        wayPointModel->setProperty("tip", transDoc->getDoc(DocIndex::RequestDrawingAgain));
    } else if (m_token == token) {
        // 如果token不为空且两次的token值相同，则认为本次校验通过
        Q_EMIT requestSaveToken(m_token);
        Q_EMIT inputFinished();
    } else {
        // 如果两次的token不同，回到上一次输入密码的界面来重新录入
        m_token.clear();
        // 给出提示，认为本次和上次校验不一致
        auto tip = wayPointModel->errorTextStyle().arg(transDoc->getDoc(DocIndex::Inconsistent));
        wayPointModel->setProperty("tip", tip);
        // 2秒过后文案变为“请绘制手势密码”
        QTimer::singleShot(2000, this, [=] {
            // 请绘制手势密码
            wayPointModel->setProperty("tip", transDoc->getDoc(DocIndex::RequestDrawing));
        });
    }
}

void GestureModifyWorker::onPathError()
{
    auto *wayPointModel = WayPointModel::instance();

    // 这里只处理录入密码的模式
    if (wayPointModel->currentMode() != Mode::Enroll)
        return;

    auto pathErrorTip = wayPointModel->errorTextStyle().arg(TranslastionDoc::instance()->getDoc(DocIndex::PathError));
    wayPointModel->setProperty("tip", pathErrorTip);

}

void GestureModifyWorker::initConnect()
{
    auto model = WayPointModel::instance();
    if (model->appType() == ModelAppType::LoginLock) {
        connect(WayPointModel::instance(), &WayPointModel::gestureStateChanged, this, [&] (int state) {
            setActive(state != GestureState::Set ? true : false);
        });
    }

    connect(model, &WayPointModel::pathDone, this, &GestureModifyWorker::onPathDone);
    connect(model, &WayPointModel::pathError, this, &GestureModifyWorker::onPathError);
}
