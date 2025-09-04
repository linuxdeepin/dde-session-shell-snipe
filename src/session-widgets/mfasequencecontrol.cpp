#include "mfasequencecontrol.h"
#include "dconfig_helper.h"
#include "authcommon.h"

#include <dconfig_helper.h>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QtMath>
#include <QWidget>

const QString kConfigAttrName = "mfaSequence";
const QString kConfigUserKey = "userType";
const QString kConfigAuthTypeKey = "authSequence";
const QString kDefaultUser = "default";
const QString kADDomainUser = "adDomain";
const QString kNativeUser = "native";
const QString kAllUser = "all";

// 不处理某个类型的认证过程，只处理UI切换
MFASequenceControl::MFASequenceControl(QObject *parent)
    : QObject(parent)
    , m_userType("")
    , m_authType(-1)
    , m_configUserType("")
    , m_currentType(-1)
    , m_validConfig(false)
{
    initConfig();
    DConfigHelper::instance()->bind(this, kConfigAttrName, &MFASequenceControl::onPropertyChanged);
}

MFASequenceControl &MFASequenceControl::instance()
{
    static MFASequenceControl control;
    return control;
}

// 外部设置当前认证类型
// 工行在此判断是否多因和是否满足配置场景
void MFASequenceControl::setAuthType(int type)
{
    qCDebug(DDE_SHELL) << "set mfa auth type" << type << this;

    // 先重置到默认界面上
    m_currentType = AuthCommon::AT_None;
    Q_EMIT currentUIAuthTypeChanged(m_currentType);

    if (type == AuthCommon::AT_None) {
        return;
    }

    m_authType = type;
    updateAuthSequence();
}

// 注意，同类型不代表同用户，所以设置后也需要更新
void MFASequenceControl::setUserType(User::UserType type)
{
    qCDebug(DDE_SHELL) << "set mfa user type" << type << this;

    static QMap<User::UserType, QString> userTypes = {
        {User::UserType::Default, kDefaultUser},
        {User::UserType::Native, kNativeUser},
        {User::UserType::ADDomain, kADDomainUser}};

    m_userType = userTypes.value(type, kDefaultUser);
}

int MFASequenceControl::currentAuthType()
{
    return m_currentType;
}

void MFASequenceControl::insertIsolateAuthWidget(int type, QWidget *authWidget)
{
    m_isolateWidgets[type] = authWidget;
}

void MFASequenceControl::removeAuthWidget(int type)
{
    m_isolateWidgets.remove(type);
}

// 设计如下：这个类仅被动响应由配置指定的认证序列
// 比如配置为[1,256], 刚以MFAWidget开始认证，当被通知认证1成功时，推出256认证的UI
// 因此这个函数只会当开始指定认证时返回非空值
QWidget *MFASequenceControl::currentAuthWidget()
{
    return m_isolateWidgets.value(m_currentType, nullptr);
}

void MFASequenceControl::doNextAuth(int finishedAuth)
{
    if (!m_configAuthList.size()) {
        return;
    }

    auto currentIndex = m_configAuthList.indexOf(finishedAuth);
    if (currentIndex == -1) {
        // 无法处理，认证类型有异常
        return;
    }

    if (currentIndex == m_configAuthList.size() - 1) {
        qCDebug(DDE_SHELL) << "mfa auth sequence done, continue to other auth";

        m_currentType = AuthCommon::AT_None;

        // 是否所有认证已结束
        auto allFinishedType = 0;
        foreach (auto type, m_configAuthList) {
            allFinishedType |= type;
        }

        // 存在配置外的认证，切回默认认证UI
        if (allFinishedType != m_authType) {
            Q_EMIT currentUIAuthTypeChanged(AuthCommon::AT_None);
        }

        return;
    }

    // 通知UI显示下一个类型的认证界面
    m_currentType = m_configAuthList.value(currentIndex + 1);
    qCDebug(DDE_SHELL) << "next auth type" << m_currentType;

    Q_EMIT currentUIAuthTypeChanged(m_currentType);
}

void MFASequenceControl::initConfig()
{
    m_validConfig = false;
    m_configAuthList.clear();
    m_configUserType = "";

    auto obj = DConfigHelper::instance()->getConfig(kConfigAttrName, "").toJsonObject();
    toLocalConfig(obj);
}

// json转内部配置
void MFASequenceControl::toLocalConfig(const QJsonObject &obj)
{
    if (!obj.contains(kConfigUserKey) || !obj.contains(kConfigAuthTypeKey)) {
        return;
    }

    // 处理用户类型
    auto nameType = obj[kConfigUserKey].toString();
    if (nameType.isEmpty()) {
        return;
    }

    m_configUserType = nameType;
    // 处理认证顺序
    auto authTypeList = obj[kConfigAuthTypeKey].toArray().toVariantList();
    // 多因必然类型比2多
    if (authTypeList.size() < 2) {
        return;
    }

    foreach (auto type, authTypeList) {
        m_configAuthList.push_back(type.toInt());
    }

    // 注意仅在完成配置后才设置true值
    m_validConfig = true;
}

// 如果配置类型在传入的类型中
// 先完成配置认证，再执行其它认证
void MFASequenceControl::updateAuthSequence()
{
    if (!m_validConfig) {
        qCDebug(DDE_SHELL) << "config not valid";
        return;
    }

    // FIXME 工行上不关注用户类型，DA仅给域用户配置
    if (!m_authType) {
        qCDebug(DDE_SHELL) << "not expect value";
        return;
    }

    if (m_configUserType != m_userType && m_configUserType != kAllUser) {
        return;
    }

    auto isolateAuthList = m_isolateWidgets.keys();
    bool isolateActive = false;
    foreach (auto isolate, isolateAuthList) {
        if (isolate & m_authType) {
            if (m_isolateWidgets.value(isolate, nullptr) != nullptr) {
                isolateActive = true;
            }
        }
    }

    // 没有参与认证的独立界面，由于认证未提供，将在认证开始后关闭
    if (!isolateActive) {
        return;
    }

    // 先完成配置的认证顺序
    m_currentType = m_configAuthList.value(0);

    qCDebug(DDE_SHELL) << "start from type" << m_currentType << m_configAuthList;
    Q_EMIT currentUIAuthTypeChanged(m_currentType);
}

void MFASequenceControl::onAuthStatusChanged(int type, int status, const QString &message)
{
    //仅用于参数对齐
    Q_UNUSED(message)

    if (!m_configAuthList.size()) {
        return;
    }

    switch (status) {
    case AuthCommon::AS_Success:
        if (type == AuthCommon::AT_All) {
            return;
        } else {
            doNextAuth(type);
        }
        break;
    case AuthCommon::AS_Started:
        // 多因认证开启了手势，但是并没有安装插件
        if (m_isolateWidgets.contains(type) && m_isolateWidgets.value(type, nullptr) == nullptr) {
            Q_EMIT requestEndUnsupportedAuth(type);
        }
    }
}

// 在下一次认证过程中生效
void MFASequenceControl::onPropertyChanged(const QString &key, const QVariant &value, QObject *objPtr)
{
    auto obj = qobject_cast<MFASequenceControl *>(objPtr);
    if (!obj)
        return;

    qCInfo(DDE_SHELL) << "DConfig property changed, key: " << key << ", value: " << value;
    if (key == kConfigAttrName) {
        obj->initConfig();
    }
}
