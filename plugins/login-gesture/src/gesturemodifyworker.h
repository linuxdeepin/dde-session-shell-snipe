// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GESTUREMODIFYWORKER_H
#define GESTUREMODIFYWORKER_H

#include <QObject>

namespace gestureSetting {

class GestureModifyWorker : public QObject
{
    Q_OBJECT

public:
    explicit GestureModifyWorker(QObject *parent = nullptr);
    void setActive(bool active);

signals:
    void inputFinished();                           // 第二次输入token合法并完成
    void requestSaveToken(const QString &token);    // 请求保存token

private slots:
    void onPathDone();
    void onPathError();

private:
    void initConnect();

private:
    QString m_token;
};

} // namespace gestureSetting

#endif // GESTUREMODIFYWORKER_H
