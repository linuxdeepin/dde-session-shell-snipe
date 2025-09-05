// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waypointmodel.h"
#include "tokenCrypt.h"

#include <QApplication>
#include <DGuiApplicationHelper>

using namespace gestureLogin;

// 明文内容字典
const QMap<int, QString> kContent = {{0, "0"},
                                     {1, "1"},
                                     {2, "2"},
                                     {3, "3"},
                                     {4, "4"},
                                     {5, "5"},
                                     {6, "6"},
                                     {7, "7"},
                                     {8, "8"}};

WayPointModel::WayPointModel(QObject *parent)
    : QObject(parent)
    , m_size(9) // 3 * 3, 如果有需求，改从配置获取
    , m_currentMode(Mode::Auth)
    , m_gestureStateFlag(-1)
    , m_gestureEnable(false)
    , m_showErrorStyle(false)
    , m_locked(false)
    , m_localeName("")
{
    initColorConfig();
    connect(this, &WayPointModel::authDone, this, &WayPointModel::clearPath);
    connect(this, &WayPointModel::authError, this, &WayPointModel::clearPath);
}

WayPointModel *WayPointModel::instance()
{
    static WayPointModel model;
    return &model;
}

const QList<int> &WayPointModel::currentPoints()
{
    return m_selectedPoints;
}

ModelAppType WayPointModel::appType() const
{
    auto name = QApplication::applicationName();
    if (name.contains("lock", Qt::CaseInsensitive) || name.contains("greeter", Qt::CaseInsensitive)) {
        return ModelAppType::LoginLock;
    }

    if (name.contains("reset", Qt::CaseInsensitive)) {
        return ModelAppType::Reset;
    }

    return ModelAppType::Unknow;
}

bool WayPointModel::isLockApp()
{
    return QApplication::applicationName().contains("lock", Qt::CaseInsensitive);
}

// 通过回调到查询
bool WayPointModel::isGestureEnabled()
{
    return m_gestureEnable;
}

const GestureColors &WayPointModel::colorConfig()
{
    return m_colorConfig;
}

QString WayPointModel::errorTextStyle() const
{
    return "<font color='#FF5336'>%1</font>";
}

void WayPointModel::setLocaleName(const QString &locale)
{
    auto content = locale.split(".");
    if (content.size()) {
        auto name = content[0];
        if (m_localeName != name) {
            m_localeName = name;
            Q_EMIT localeChanged(m_localeName);
        }
    }
}

void WayPointModel::initColorConfig()
{
    DGUI_USE_NAMESPACE;
    // 登录锁屏样式无大变化
    if (appType() == ModelAppType::LoginLock) {
        setColorByTheme(DGuiApplicationHelper::UnknownType);
    } else {
        // 对话框上存在明暗样式
        auto themeType = DGuiApplicationHelper::instance()->themeType();
        setColorByTheme(themeType);

        connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [&] (DGuiApplicationHelper::ColorType t) {
            setColorByTheme(t);
            Q_EMIT widgetColorChanged();
        });
    }
}

void WayPointModel::setColorByTheme(int type)
{
    DGUI_USE_NAMESPACE;
    switch (type) {
    case DGuiApplicationHelper::LightType:
        m_colorConfig.line.setRgb(0, 129, 255);
        m_colorConfig.unSelecteBoarder.setRgb(0, 0, 0, 0.3 * 255);
        m_colorConfig.selectedBoarder.setRgb(0, 129, 255);
        m_colorConfig.fill.setRgb(0, 129, 255, 0.1 * 255);
        m_colorConfig.internalFill.setRgb(0, 129, 255);
        m_colorConfig.warningFill.setRgb(255, 87, 54, 0.1 * 255);
        m_colorConfig.warningLine.setRgb(255, 87, 54);
        break;
    case DGuiApplicationHelper::DarkType:
        m_colorConfig.line.setRgb(0, 129, 255);
        m_colorConfig.unSelecteBoarder.setRgb(255, 255, 255, 0.3 * 255);
        m_colorConfig.selectedBoarder.setRgb(0, 129, 255);
        m_colorConfig.fill.setRgb(0, 129, 255, 0.1 * 255);
        m_colorConfig.internalFill.setRgb(0, 129, 255);
        m_colorConfig.warningFill.setRgb(255, 87, 54, 0.1 * 255);
        m_colorConfig.warningLine.setRgb(255, 87, 54);
        break;
    default:
        m_colorConfig.line.setRgb(255, 255, 255);
        m_colorConfig.unSelecteBoarder.setRgb(255, 255, 255, 0.74 * 255);
        m_colorConfig.selectedBoarder.setRgb(255, 255, 255, 0.74 * 255);
        m_colorConfig.fill.setRgb(255, 255, 255, 0.2 * 255);
        m_colorConfig.internalFill.setRgb(255, 255, 255);
        m_colorConfig.warningFill = m_colorConfig.fill;
        m_colorConfig.warningLine = m_colorConfig.line;
        break;
    }
}

QString WayPointModel::userName() const
{
    return m_userName;
}

void WayPointModel::setUserName(const QString &name)
{
    m_userName = name;
}

bool WayPointModel::isValidPath() const
{
    auto size = m_selectedPoints.size();
    return size >= 4;
}

QString WayPointModel::getToken() const
{
    qDebug() << int(m_currentMode);
    QString rst = "";
    foreach (auto index, m_selectedPoints) {
        rst.append(kContent.value(index, ""));
    }

    if (rst.isEmpty()) {
        qDebug() << "skip crypt for empty input";
        return "";
    }

    if (appType() == ModelAppType::Reset && m_currentMode == Mode::Auth)
        return rst;

    // 仅录入时加密，登录、解锁时由前端处理明文
    // 录入时两次需要使用相同盐值加密
    if (m_currentMode == Mode::Enroll || appType() == ModelAppType::Reset) {
        static QString salt = gestureEncrypt::getSalt();
        rst = gestureEncrypt::cryptUserPassword(rst, salt.toLatin1());
    }

    return rst;
}

Mode WayPointModel::currentMode() const
{
    return m_currentMode;
}

void WayPointModel::setCurrentMode(Mode mode)
{
    m_currentMode = mode;
    Q_EMIT modeChanged(m_currentMode);

    qDebug() << "model mode changed to:" << int(m_currentMode);
}

void WayPointModel::setTitle(const QString &title)
{
    m_title = title;
    Q_EMIT titleChanged(m_title);
}

void WayPointModel::setTip(const QString &tip)
{
    m_tip = tip;
    Q_EMIT tipChanged(m_tip);
}

void WayPointModel::setGestureState(int flag)
{
    qDebug() << flag << m_gestureStateFlag;
    if (m_gestureStateFlag != flag) {
        m_gestureStateFlag = flag;
        Q_EMIT gestureStateChanged(m_gestureStateFlag);
    }
}

void WayPointModel::setGestureEnabled(bool enable)
{
    m_gestureEnable = enable;
}

int WayPointModel::getGestureState()
{
    return m_gestureStateFlag;
}

void WayPointModel::onPathArrived(int index)
{
    if (index < 0) {
        return;
    }

    do {
        const int current = index;
        if (m_selectedPoints.contains(current)) {
            // 如果索引与倒数第二个相同，代表撤消上一个选中
            if (m_selectedPoints.size() >= 2) {
                if (m_selectedPoints.indexOf(current) == m_selectedPoints.size() - 2) {
                    // 先发信号再移除，注意顺序
                    Q_EMIT selected(m_selectedPoints.last(), false);
                    m_selectedPoints.removeLast();

                    Q_EMIT dataChanged();
                }
            }
            break;
        }

        // 不包含，判断是否有途经的点并优先插入
        if (!m_selectedPoints.size()) {
            appendPath(current);
            break;
        }

        if (m_selectedPoints.size()) {
            // 选中所有沿途点
            int currentX = current % 3;
            int currentY = current / 3;

            auto last = m_selectedPoints.last();
            int lastX = last % 3;
            int lastY = last / 3;

            auto step = (current - last) / qAbs(current - last);

            auto insertPassby = [&](int dx, int dy) {
                auto passByX = lastX + dx;
                auto passByY = lastY + dy;

                while (passByY != currentY || passByX != currentX) {
                    int passbyIndex = positionToIndex(passByX, passByY);
                    if (passbyIndex >= 0) {
                        appendPath(passbyIndex);
                    }
                    passByX += dx;
                    passByY += dy;
                }
            };

            if (currentX == lastX) {
                // 同列
                insertPassby(0, step);
            } else if (currentY == lastY) {
                // 同行
                insertPassby(step, 0);
            } else if (qAbs(currentX - lastX) == qAbs(currentY - lastY)) {
                // 同斜率
                auto stepX = (currentX - lastX) / qAbs(currentX - lastX);
                auto stepY = (currentY - lastY) / qAbs(currentY - lastY);
                insertPassby(stepX, stepY);
            }

            // 插入路过的点后，判断当前点是否在期望的路径上
            last = m_selectedPoints.last();
            lastX = last % 3;
            lastY = last / 3;
            if (currentX == lastX || currentY == lastY || qAbs(currentX - lastX) == qAbs(currentY - lastY)) {
                appendPath(current);
            }
        }

    } while (false);
}

void WayPointModel::onUserInputDone()
{
    if (m_selectedPoints.size() < 4 || m_selectedPoints.size() > 9) {
        m_showErrorStyle = true;
        clearPath();
        Q_EMIT pathError();
    } else {
        Q_EMIT pathDone();
    }
}

void WayPointModel::clearPath()
{
    m_selectedPoints.clear();
    Q_EMIT selected(-1, false); // 清空所有结点
    Q_EMIT dataChanged();

    m_showErrorStyle = false;
}

void WayPointModel::setLocked(bool locked)
{
    if (m_locked != locked) {
        m_locked = locked;
        Q_EMIT inputLocked(m_locked);
    }
}

int WayPointModel::positionToIndex(int x, int y)
{
    if (x < 0 || y < 0) {
        return -1;
    }

    int index = static_cast<int>(m_size) - 1;
    while (index >= 0) {
        int ix = index % 3;
        int iy = index / 3;
        if (ix == x && iy == y) {
            return index;
        }

        index--;
    }
    return -1;
}

void WayPointModel::appendPath(int index)
{
    if (!m_selectedPoints.contains(index)) {
        m_selectedPoints.push_back(index);
        Q_EMIT selected(index, true);
        Q_EMIT dataChanged();
    }
}
