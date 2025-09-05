#ifndef MFASEQUENCECONTROL_H
#define MFASEQUENCECONTROL_H

#include "userinfo.h"

#include <QObject>
#include <QList>

class MFASequenceControl : public QObject
{
    Q_OBJECT
public:
    static MFASequenceControl &instance();
    void setAuthType(int);
    void setUserType(User::UserType);

    // 外部传入widget，如果为空也代表认证无效
    // 内置认证不参与
    void insertIsolateAuthWidget(int, QWidget*);

    int currentAuthType();
    QWidget *currentAuthWidget();

    void doNextAuth(int);

public Q_SLOTS:
    void initConfig();
    // 响应认证状态变化
    void onAuthStatusChanged(int, int, const QString &);
    // 响应配置变化
    static void onPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr);

Q_SIGNALS:
    // 当某个认证成功时，通知UI转到下一个认证
    void currentUIAuthTypeChanged(int);

    // 某个认证的UI不存在，通知结束该认证
    void requestEndUnsupportedAuth(int);

private:
    explicit MFASequenceControl(QObject *parent = nullptr);

    // 从dconf初始化，greeter与lock各持一份，区别配置
    void toLocalConfig(const QJsonObject &);
    void updateAuthSequence();
    void removeAuthWidget(int);

private:
    // 外部设置当前用户与开启的认证类型
    QString m_userType;
    int m_authType;

    // 实际配置值
    QString m_configUserType;
    QList<int> m_configAuthList;

    // 当前处理中的认证
    int m_currentType;

    // 配置可靠性
    bool m_validConfig;

    // 外部已注册的提供UI的认证
    QMap<int, QPointer<QWidget>> m_isolateWidgets;
};

#endif // MFASEQUENCECONTROL_H
