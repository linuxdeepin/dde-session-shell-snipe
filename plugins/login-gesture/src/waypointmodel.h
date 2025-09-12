// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WAYPOINTMODEL_H
#define WAYPOINTMODEL_H

#include <QObject>
#include <QList>
#include <QColor>

namespace gestureLogin {

enum GestureState {
    NotSet,
    SetAndRemoved,
    Set
};

enum class Mode {
    Enroll,
    Auth
};

enum class ModelAppType {
    LoginLock,
    Reset,
    Unknow
};

struct GestureColors {
    QColor line;
    QColor unSelecteBoarder;
    QColor selectedBoarder;
    QColor fill;
    QColor internalFill;
    QColor warningFill;
    QColor warningLine;
};

class WayPointModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString title MEMBER m_title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString tip MEMBER m_tip WRITE setTip NOTIFY tipChanged)

public:
    static WayPointModel *instance();
    const QList<int> &currentPoints();

    ModelAppType appType() const;
    bool isLockApp();

    // 响应userSwitch，重置校验过程与状态
    // 通过用户名查询user的dbuspath
    QString userName() const;
    void setUserName(const QString &);

    inline uint size() const { return m_size; }

    bool isValidPath() const;
    QString getToken() const;

    Mode currentMode() const;
    void setCurrentMode(Mode);

    void setTitle(const QString &);
    void setTip(const QString &);

    inline QString title() const { return m_title; }
    inline QString tip() const { return m_tip; }
    inline bool locked() const { return m_locked; }

    void setGestureState(int);
    void setGestureEnabled(bool);

    int getGestureState();
    bool isGestureEnabled();

    inline bool showErrorStyle() { return m_showErrorStyle; }
    const GestureColors& colorConfig();

    QString errorTextStyle() const;

    void setLocaleName(const QString &);
    inline QString localeName() const { return m_localeName; }

public Q_SLOTS:
    void onPathArrived(int);
    void onUserInputDone();
    void clearPath();
    void setLocked(bool);

Q_SIGNALS:
    void inputLocked(bool);

    // 文案控制
    void titleChanged(const QString &);
    void tipChanged(const QString &);
    void widgetColorChanged();

    void modeChanged(Mode);
    // 录入
    void finished();

    void pathError(); // 不满足条件
    void pathDone(); // 满足条件

    // 校验的信号转发
    void authError(); // 验证失败
    void authDone(); // 验证成功

    void dataChanged(); // 通知UI执行更新
    void selected(int, bool); // 单个点被选中或取消

    void gestureStateChanged(int);
    void localeChanged(const QString &);

private:
    explicit WayPointModel(QObject *parent = nullptr);
    int positionToIndex(int, int);
    void appendPath(int);
    void removePath(int);
    void initColorConfig();
    void setColorByTheme(int);

private:
    QList<int> m_selectedPoints;
    uint m_size;
    Mode m_currentMode;

    QString m_userName;
    QString m_title;
    QString m_tip;
    // 后端手势标志： 0 从未录入 1 已被重置 2 已录入
    int m_gestureStateFlag;
    bool m_gestureEnable;
    bool m_showErrorStyle;
    GestureColors m_colorConfig;
    bool m_locked;
    QString m_localeName;
};
} // namespace gestureLogin

#endif // WAYPOINTMODEL_H
