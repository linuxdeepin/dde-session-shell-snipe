// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "translastiondoc.h"
#include "waypointmodel.h"

#include <QTranslator>
#include <QLocale>
#include <QApplication>
#include <QDebug>
#include <QFile>

const QString kTransFile = "%1/login-gesture_%2.qm";

using namespace gestureLogin;

TranslastionDoc::TranslastionDoc(QObject *parent)
    : QObject(parent)
    , m_translator(new QTranslator(this))
{
    setLocale(WayPointModel::instance()->localeName());
    connect(WayPointModel::instance(), &WayPointModel::localeChanged, this, &TranslastionDoc::setLocale);
}

TranslastionDoc *TranslastionDoc::instance()
{
    static TranslastionDoc doc;
    return &doc;
}

QString TranslastionDoc::getDoc(DocIndex index)
{
    return m_doc.value(index, "");
}

void TranslastionDoc::setLocale(const QString &localName)
{
    auto name = localName;
    if (name.isEmpty()) {
        name = QLocale::system().name();
    }

    qApp->removeTranslator(m_translator);
    QString transFile = kTransFile.arg(QM_FILES_DIR).arg(name);
    if (QFile(transFile).exists()) {
        m_translator->load(transFile);
        qApp->installTranslator(m_translator);
        initDoc();
    } else {
        qWarning() << transFile << "not exists";
    }
}

void TranslastionDoc::initDoc()
{
    m_doc.clear();
    m_doc[DocIndex::Enabled] = tr("Gesture password is enabled");
    m_doc[DocIndex::RequestFirstEnroll] = tr("For first time use, please set the gesture password first");
    m_doc[DocIndex::SetPasswd] = tr("Set gesture password");
    m_doc[DocIndex::RequestDrawing] = tr("Please draw a gesture password");
    m_doc[DocIndex::RequestDrawingAgain] = tr("Please draw the gesture password again");
    m_doc[DocIndex::ForgotPasswd] = tr("Forgot gesture password");
    m_doc[DocIndex::PasswdCleared] = tr("Gesture password has been reset");
    m_doc[DocIndex::NeedResetPasswd] = tr("Please reset the gesture password");
    m_doc[DocIndex::PathError] = tr("Minimum 4 points, please redraw");
    m_doc[DocIndex::Inconsistent] = tr("Inconsistent with the last drawing, please redraw");
    m_doc[DocIndex::ContactAdmin] = tr("Drawing error, Contact the administrator to reset");
    m_doc[DocIndex::DeviceLock] = tr("Device is locked, unlocked after %1 minutes");
    m_doc[DocIndex::ModifyPasswd] = tr("Modify gesture password");
    m_doc[DocIndex::DrawCurrentPasswd] = tr("Please draw the current gesture password");
    m_doc[DocIndex::EnrollDone] = tr("Modified successfully");
    m_doc[DocIndex::Cancel] = tr("Cancel");
    m_doc[DocIndex::Ok] = tr("Ok");
    m_doc[DocIndex::AuthErrorWithTimes] = tr("Drawing error, %1 chances left. Contact the administrator to reset");
    m_doc[DocIndex::DeviceLockHelp] = tr("Contact the administrator to reset");

    // 登录器与锁屏的差异部分
    if (WayPointModel::instance()->isLockApp()) {
        m_doc[DocIndex::LoginAfterEnroll] = tr("Setup completed Start unlock");
        m_doc[DocIndex::LoginStart] = tr("Unlock with gesture password");
    } else {
        m_doc[DocIndex::LoginAfterEnroll] = tr("Setup completed Start login");
        m_doc[DocIndex::LoginStart] = tr("Sign in with gesture password");
    }
}
