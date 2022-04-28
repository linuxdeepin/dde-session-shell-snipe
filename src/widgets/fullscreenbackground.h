/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FULLSCREENBACKGROUND_H
#define FULLSCREENBACKGROUND_H

#include <QWidget>
#include <QSharedPointer>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(DDE_SS)

#include <com_deepin_daemon_imageeffect.h>

using ImageEffectInter = com::deepin::daemon::ImageEffect;

class BlackWidget;
class SessionBaseModel;
class FullscreenBackground : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool contentVisible READ contentVisible WRITE setContentVisible NOTIFY contentVisibleChanged)

public:
    explicit FullscreenBackground(SessionBaseModel *model, QWidget *parent = nullptr);
    ~FullscreenBackground() override;

    bool contentVisible() const;
    void setEnterEnable(bool enable);

public slots:
    void updateBackground(const QString &path);
    void updateBlurBackground(const QString &path);
    void setScreen(QPointer<QScreen> screen, bool isVisible = true);
    void setContentVisible(bool visible);
    void setIsHibernateMode();

signals:
    void contentVisibleChanged(bool contentVisible);
    void requestDisableGlobalShortcutsForWayland(bool enable);

protected:
    void setContent(QWidget *const w);
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *e) override;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *event) Q_DECL_OVERRIDE;

private:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    const QPixmap pixmapHandle(const QPixmap &pixmap);
    void updateScreen(QPointer<QScreen> screen);
    void updateGeometry();
    bool isPicture(const QString &file);
    QString getLocalFile(const QString &file);
    const QPixmap& getPixmap(int type);
    QSize trueSize() const;
    void addPixmap(const QPixmap &pixmap, const int type);
    static void updatePixmap();
    bool contains(int type);
    void tryActiveWindow(int count = 9);

private:
    static QString backgroundPath;                             // 高清背景图片路径
    static QString blurBackgroundPath;                         // 模糊背景图片路径

    static QList<QPair<QSize, QPixmap>> backgroundCacheList;
    static QList<QPair<QSize, QPixmap>> blurBackgroundCacheList;
    static QList<FullscreenBackground *> frameList;

    QVariantAnimation *m_fadeOutAni;      // 背景动画
    ImageEffectInter *m_imageEffectInter; // 获取模糊背景服务

    QPointer<QWidget> m_content;
    QPointer<QScreen> m_screen;
    SessionBaseModel *m_model = nullptr;
    bool m_primaryShowFinished = false;
    bool m_enableEnterEvent = true;
    bool m_useSolidBackground;
    bool m_fadeOutAniFinished;
    bool m_enableAnimation;

    BlackWidget *m_blackWidget;
};

#endif // FULLSCREENBACKGROUND_H
