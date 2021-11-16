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
#include <QPointer>
#include <QTimer>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QVariantAnimation>

#include <com_deepin_daemon_imageeffect.h>
#include <com_deepin_wm.h>

using ImageEffectInter = com::deepin::daemon::ImageEffect;
using WMInter = com::deepin::wm;

class FullscreenBackground : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool contentVisible READ contentVisible WRITE setContentVisible NOTIFY contentVisibleChanged)

public:
    enum DisplayMode {
        CopyMode = 1,
        ExpandMode = 2
    };

    explicit FullscreenBackground(QWidget *parent = nullptr);
    ~FullscreenBackground();
    bool contentVisible() const;
    void enableEnterEvent(bool enable);

public slots:
    void updateBackground(const QPixmap &background);
    void updateBackground(const QString &file = QString());
    void updateWMBackground();
    void setScreen(QScreen *screen);
    void setContentVisible(bool contentVisible);
    void setIsBlackMode(bool is_black);
    void setIsHibernateMode();

signals:
    void contentVisibleChanged(bool contentVisible);

protected:
    void setContent(QWidget * const w);
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *e) override;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    const QPixmap pixmapHandle(const QPixmap &pixmap);

private:
    void updateScreen(QScreen *screen);
    void updateGeometry();
    void updateBlurBackground(const QString &file);
    bool isPicture(const QString &file);
    using QWidget::setGeometry;
    using QWidget::resize;
    using QWidget::move;

    QPixmap m_background;
    QPixmap m_fakeBackground;
    QPixmap m_backgroundCache;
    QPixmap m_fakeBackgroundCache;
    QPointer<QWidget> m_content;
    QVariantAnimation *m_fadeOutAni;
    QScreen *m_screen = nullptr;
    ImageEffectInter *m_imageEffectInter = nullptr;
    WMInter *m_wmInter;
    QString m_wmBackgroundPath;
    bool m_primaryShowFinished = false;
    bool m_isBlackMode = false;
    bool m_isHibernateMode = false;
    bool m_enableEnterEvent = true;
    QCursor m_originalCursor;
    int m_displayMode;
};

#endif // FULLSCREENBACKGROUND_H
