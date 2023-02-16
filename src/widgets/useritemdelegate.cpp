// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "useritemdelegate.h"

#include <DFontSizeManager>

#include <QFile>
#include <QPainter>
#include <QBitmap>
#include <QPainterPath>

DWIDGET_USE_NAMESPACE

const int ITEM_HEIGHT = 64;
const int RADIUS_VALUE = 8;
const QSize IMAGE_SIZE = QSize(48, 48);

UserItemDelegate::UserItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{

}

void UserItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    QPen pen;
    QRect rect = option.rect;

    if (option.state.testFlag(QStyle::State_Selected)) {
        // 鼠标悬停背景色
        QColor hoverColor(0, 129, 255, int(0.9 * 255));
        pen.setColor(hoverColor);
        painter->setPen(pen);
        painter->setBrush(hoverColor);
    } else {
        // 默认背景颜色
        QColor bgColor(Qt::black);
        bgColor.setAlpha(0.05 * 255);
        pen.setColor(bgColor);
        painter->setPen(pen);
        painter->setBrush(bgColor);
    }

    // 绘制背景颜色
    painter->drawRoundedRect(rect, RADIUS_VALUE, RADIUS_VALUE);

    painter->setRenderHint(QPainter::Antialiasing, true);

    UserItemData userData = index.data(StaticUserDataRole).value<UserItemData>();

    // 绘制圆角图像
    drawRoundImage(painter, rect, userData.imagePath);

    // 已登录标志
    if (userData.isLogined)
        drawLoginedState(painter, rect);

    if (option.state.testFlag(QStyle::State_Selected)) {
        pen.setColor(Qt::white);
    } else {
        pen.setColor(Qt::black);
    }
    painter->setPen(pen);

    // 绘制userName，fullName，用户类型
    drawNameAndType(painter, userData, rect);

    bool isCurrentUser = index.data(IsCurrentUserDataRole).toBool();
    if (isCurrentUser)
        drawCheckedState(painter, rect);
}

QSize UserItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(option.rect.width(), ITEM_HEIGHT);
}

void UserItemDelegate::drawRoundImage(QPainter *thisPainter, const QRect &rect, const QString &path) const
{
    if (path.isEmpty() || !QFile::exists(path))
        return;

    // 设计图上常量
    const int margTop = 8;
    const int marginLeft = 8;
    const int imageRadius = 10;

    QRect drawRect = QRect(rect.left() + marginLeft, rect.top() + margTop,
                           IMAGE_SIZE.width(), IMAGE_SIZE.height());

    // 画成圆角图片
    QBitmap mask(IMAGE_SIZE);
    QPainter painter(&mask);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.fillRect(0, 0, IMAGE_SIZE.width(), IMAGE_SIZE.height(), Qt::white);
    painter.setBrush(Qt::black);

    // 修改后面两个数值，可以改弧度
    painter.drawRoundedRect(0, 0, IMAGE_SIZE.width(), IMAGE_SIZE.height(), imageRadius, imageRadius);

    QPixmap pixmap(path);
    pixmap = pixmap.scaled(IMAGE_SIZE);
    pixmap.setMask(mask);

    thisPainter->drawPixmap(drawRect, pixmap);
}

void UserItemDelegate::drawLoginedState(QPainter *painter, const QRect &rect) const
{
    const int leftMargin = 50;
    const int topMargin = 50;
    const int radiusWidth = 8;

    QRect drawRect = QRect(rect.left() + leftMargin, rect.top() + topMargin,
                           radiusWidth, radiusWidth);
    QPen pen;
    pen.setColor(Qt::white);
    painter->setPen(pen);
    painter->setBrush(QBrush(Qt::green));

    painter->drawEllipse(drawRect);
}

void UserItemDelegate::drawNameAndType(QPainter *painter, const UserItemData &userData, const QRect &rect) const
{
    // 文字区域设计图常量
    const int leftMargin = 65;  // 开始位置距离item最左边
    const int rightMargin = 48; // 结束位置距离item最右边
    const int topMargin = 6;
    const int displayNameAreaHeight = 29;
    const int itemSpacing = 10;
    const int nameAreaHeight = 20;
    const int typeAreaHeight = 17;
    const int vItemSpacing = 2;

    int textAreaWidth = rect.width() - leftMargin - rightMargin;
    QRect displayNameRect = QRect(rect.left() + leftMargin, rect.top() + topMargin,
                                  textAreaWidth, displayNameAreaHeight);

    QFont font = DFontSizeManager::instance()->t4();
    font.setBold(true);
    painter->setFont(font);

    // 绘制displayName, +1个像素是为了避免刚好width和文本宽度一致，最后文本还有省略号
    QString displayNameText = elidedText(userData.displayName, textAreaWidth + 1, font.pixelSize(), true);
    painter->drawText(displayNameRect, displayNameText);

    QFont typeFont = DFontSizeManager::instance()->t8();
    int userTypeAreaWidth = stringWidth(userData.userType, typeFont.pixelSize());

    // 绘制name
    int nameWidth = 0;
    if (!userData.name.isEmpty()) {
        font = DFontSizeManager::instance()->t6();
        font.setBold(false);
        painter->setFont(font);

        QString nameText = elidedText(userData.name, textAreaWidth - itemSpacing - userTypeAreaWidth, font.pixelSize());
        nameWidth = stringWidth(nameText, font.pixelSize());
        QRect nameRect = QRect(displayNameRect.left(), displayNameRect.bottom() + vItemSpacing,
                                   textAreaWidth - itemSpacing - userTypeAreaWidth, nameAreaHeight);

        painter->drawText(nameRect, nameText);
    }

    typeFont.setBold(false);
    painter->setFont(typeFont);

    // 绘制userType, +1个像素为了避免字体大小不一样，绘制中心位置不一致
    int userTypeLeft = userData.name.isEmpty() ? displayNameRect.left() : displayNameRect.left() + nameWidth + itemSpacing;
    QRect userTypeRect = QRect(userTypeLeft, displayNameRect.bottom() + vItemSpacing + 1, userTypeAreaWidth, typeAreaHeight);
    painter->drawText(userTypeRect, userData.userType);
}

void UserItemDelegate::drawCheckedState(QPainter *painter, const QRect &rect) const
{
    // 绘制对勾,14x12
    const int width = 14;
    const int height = 12;
    const int marginRight = 11;

    QPen pen = painter->pen();
    pen.setWidth(2);
    painter->setPen(pen);
    painter->setBrush(QBrush());

    int left = rect.width() - marginRight - width;
    int top = rect.top() + rect.height() / 2 - height / 2;

    QPainterPath path;
    path.moveTo(left, top + 6);
    path.lineTo(left + 4, top + 11);
    path.lineTo(left + 12, top + 1);

    painter->drawPath(path);
}

int UserItemDelegate::stringWidth(const QString &str, int fontSize, bool isBold) const
{
    if (str.isEmpty())
        return 0;

    QFont font;
    font.setBold(isBold);
    font.setPixelSize(fontSize);

    QFontMetrics fm(font);
    return fm.horizontalAdvance(str);
}

QString UserItemDelegate::elidedText(const QString &originString, int width, int fontSize, bool isBold) const
{
    if (originString.isEmpty())
        return QString();

    QFont font;
    font.setBold(isBold);
    font.setPixelSize(fontSize);

    QFontMetrics fm(font);
    return fm.elidedText(originString, Qt::ElideRight, width);
}
