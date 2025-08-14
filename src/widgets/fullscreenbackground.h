// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FULLSCREENBACKGROUND_H
#define FULLSCREENBACKGROUND_H

#include <QWidget>
#include <QSharedPointer>
#include <QLoggingCategory>
#include <QSize>
#include <QPointer>
#include <QVariantAnimation>

#include "abstractfullbackgroundinterface.h"
#include "shutdown_black_widget.h"

Q_DECLARE_LOGGING_CATEGORY(DDE_SS)

class BlackWidget;
class SessionBaseModel;
class FullScreenBackground : public QWidget, public AbstractFullBackgroundInterface
{
    Q_OBJECT
    Q_PROPERTY(bool contentVisible READ contentVisible)

public:
    explicit FullScreenBackground(SessionBaseModel *model, QWidget *parent = nullptr);
    ~FullScreenBackground() override;

    bool contentVisible() const;
    void setEnterEnable(bool enable);
    static void setContent(QWidget *const w);

public slots:
    void updateBackground(const QString &path);
    void updateBlurBackground(const QString &path);
    void setScreen(QPointer<QScreen> screen, bool isVisible = true) override;
    void setIsHibernateMode();

signals:
    void requestDisableGlobalShortcutsForWayland(bool enable);
    void requestLockFrameHide();

protected:
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *e) override;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void moveEvent(QMoveEvent *event) Q_DECL_OVERRIDE;

private:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) Q_DECL_OVERRIDE;
#else
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
#endif
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
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
    QMap<QString, QRect> getScreenGeometryByXrandr();
    double getScaleFactorFromDisplay();
    static void updateCurrentFrame(FullScreenBackground *frame);
    bool getScaledBlurImage(const QString &originPath, QString &scaledPath);
    void setddeGeometry(const QRect &rect);
    void updateScreenBluBackground(const QString &path);

protected:
    static QPointer<QWidget> currentContent;
    static QList<FullScreenBackground *> frameList;
    static QPointer<FullScreenBackground> currentFrame;

    void handleBackground(const QString &path, int type);
    static QString sizeToString(const QSize &size);

private:
    static QString originBackgroundPath; // 原图路径
    static QString blurBackgroundPath; // 模糊背景图片路径
    static QMap<QString, QPixmap> blurBackgroundCacheMap;

    QPointer<QScreen> m_screen;
    SessionBaseModel *m_model = nullptr;
    bool m_enableEnterEvent = true;
    bool m_useSolidBackground;

    BlackWidget *m_blackWidget;
    QTimer *m_resetGeometryTimer;
    QRect m_geometryRect;
    ShutdownBlackWidget *m_shutdownBlackWidget;
};

#endif // FULLSCREENBACKGROUND_H
