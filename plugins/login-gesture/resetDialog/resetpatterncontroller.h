// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESETPATTERNCONTROLLER_H
#define RESETPATTERNCONTROLLER_H

#include <QObject>

class QFile;

namespace gestureLogin {
class GestureAuthWorker;
}

namespace gestureSetting {
class GestureModifyWorker;
}

class ResetPatternController : public QObject
{
    Q_OBJECT

public:
    explicit ResetPatternController(QObject *parent = nullptr);
    ~ResetPatternController() override;
    void setOldPasswordFileId(int fileId);
    void setNewPasswordFileId(int fileId);
    void start();
    void close();

signals:
    void inputFinished();                           // 第二次输入token合法并完成

private:
    void initConnection();

private slots:
    void onSaveToken(const QString &token);

private:
    int m_fileId;
    QString m_localPassword;
    QFile *m_file;
    gestureLogin::GestureAuthWorker *m_authWorker;
    gestureSetting::GestureModifyWorker *m_modifyWorker;
};

#endif // RESETPATTERNCONTROLLER_H
