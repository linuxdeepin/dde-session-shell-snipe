// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "resetpatterncontroller.h"
#include "gestureauthworker.h"
#include "gesturemodifyworker.h"
#include "waypointmodel.h"
#include "translastiondoc.h"

#include <QFile>
#include <QDebug>

using namespace gestureLogin;
using namespace gestureSetting;

ResetPatternController::ResetPatternController(QObject *parent)
    : QObject(parent)
    , m_fileId(0)
    , m_file(new QFile(this))
    , m_authWorker(new gestureLogin::GestureAuthWorker(this))
    , m_modifyWorker(new gestureSetting::GestureModifyWorker(this))
{
    initConnection();
}

ResetPatternController::~ResetPatternController()
{
}

void ResetPatternController::setOldPasswordFileId(int fileId)
{
    QFile file;
    if (!file.open(fileId, QFile::ReadOnly)) {
        qWarning() << "Failed to open file for writing, fileId:" << fileId;
        return;
    }
    m_localPassword = file.readLine();
    // 需要去掉后面的\n，否则无法校验通过
    m_localPassword = m_localPassword.trimmed();
    m_authWorker->setLocalPassword(m_localPassword);
}

void ResetPatternController::setNewPasswordFileId(int fileId)
{
    m_fileId = fileId;
}

void ResetPatternController::start()
{
    auto *wayPointModel = WayPointModel::instance();

    if (m_localPassword.isEmpty()) {
        // 如果本地密码为空，则不进入校验阶段，直接请求密码
        wayPointModel->setCurrentMode(Mode::Enroll);
        m_modifyWorker->setActive(true);
    } else {
        // 如果本地密码不为空，则先进行本地密码校验
        wayPointModel->setCurrentMode(Mode::Auth);
        m_authWorker->setActive(true);
    }
}

void ResetPatternController::close()
{
    if (m_file->isOpen()) {
        m_file->close();
    }
}

void ResetPatternController::initConnection()
{
    auto *wayPointModel = WayPointModel::instance();

    connect(wayPointModel, &WayPointModel::authDone, this, [this, wayPointModel] {
        // 如果当前是重置密码模式，不做任何处理
        if (wayPointModel->currentMode() == Mode::Enroll)
            return;

        // 停止认证的worker
        m_authWorker->setActive(false);
        // 密码验证通过后，设置当前为重置密码模式
        wayPointModel->setCurrentMode(Mode::Enroll);
        // 进入设置密码页面后，设置标题和提示内容
        m_modifyWorker->setActive(true);
    });

    connect(m_modifyWorker, &GestureModifyWorker::inputFinished, this, &ResetPatternController::inputFinished);
    connect(m_modifyWorker, &GestureModifyWorker::requestSaveToken, this, &ResetPatternController::onSaveToken);
}

void ResetPatternController::onSaveToken(const QString &token)
{
    // 向父进程写入token
    if (m_file->isOpen())
        m_file->close();

    if (!m_file->open(m_fileId, QFile::WriteOnly)) {
        qWarning() << "Failed to open file for writing, fileId:" << m_fileId;
        return;
    }

    qDebug() << "Writing token";
    QByteArray tmpData = QString("%1\n").arg(token).toLocal8Bit();
    m_file->write(tmpData, tmpData.length());
}
