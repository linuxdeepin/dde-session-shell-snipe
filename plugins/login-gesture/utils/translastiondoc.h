// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRANSLASTIONDOC_H
#define TRANSLASTIONDOC_H

#include <QObject>
#include <QMap>

class QTranslator;

namespace gestureLogin {

enum class DocIndex {
    Enabled,
    RequestFirstEnroll,
    SetPasswd,
    RequestDrawing,
    RequestDrawingAgain,
    LoginAfterEnroll,
    ForgotPasswd,
    LoginStart,
    GestureLockStart,
    PasswdCleared,
    NeedResetPasswd,
    PathError,
    Inconsistent,
    GestureAuthError,
    ContactAdmin,
    DeviceLock,
    ModifyPasswd,
    DrawCurrentPasswd,
    EnrollDone,
    Cancel,
    Ok,
    AuthErrorWithTimes,
    DeviceLockHelp
};

// 在这里加载翻译文件，提供翻译内容
class TranslastionDoc : public QObject
{
    Q_OBJECT

public:
    static TranslastionDoc *instance();
    QString getDoc(DocIndex);

public Q_SLOTS:
    void setLocale(const QString &);

private:
    explicit TranslastionDoc(QObject *parent = nullptr);
    void initDoc();

private:
    QMap<DocIndex, QString> m_doc;
    QTranslator *m_translator;
};
} // namespace gestureLogin
#endif // TRANSLASTIONDOC_H
