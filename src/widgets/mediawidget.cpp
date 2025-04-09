// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediawidget.h"
#include "util_updateui.h"
#include "constants.h"
#include "dbusmediaplayer2.h"

#include <QHBoxLayout>
#include <QWheelEvent>

MediaWidget::MediaWidget(QWidget *parent) : QWidget(parent)
    , m_dmprisWidget(nullptr)
{
}

void MediaWidget::initUI()
{
    if (m_dmprisWidget)
        return;
    m_dmprisWidget = new DMPRISControl;
    m_dmprisWidget->setAccessibleName("MPRISWidget");
    m_dmprisWidget->setFixedHeight(DDESESSIONCC::LOCK_CONTENT_TOP_WIDGET_HEIGHT);
    m_dmprisWidget->setPictureVisible(false);

    auto mainLayout = new QVBoxLayout;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    mainLayout->setContentsMargins(0, 0, 0, 0);
#else
    mainLayout->setMargin(0);
#endif
    mainLayout->addWidget(m_dmprisWidget);
    setLayout(mainLayout);

    initConnect();
}

void MediaWidget::initConnect()
{
    connect(m_dmprisWidget, &DMPRISControl::mprisAcquired, this, &MediaWidget::changeVisible);
    connect(m_dmprisWidget, &DMPRISControl::mprisLosted, this, &MediaWidget::changeVisible);

    changeVisible();
}

void MediaWidget::initMediaPlayer()
{
    QDBusInterface dbusInter("org.freedesktop.DBus", "/", "org.freedesktop.DBus", QDBusConnection::sessionBus(), this);

    QDBusPendingCall call = dbusInter.asyncCall("ListNames");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [=] {
        if (!call.isError()) {
            QDBusReply<QStringList> reply = call.reply();
            const QStringList &serviceList = reply.value();
            QString service = QString();
            for (const QString &serv : serviceList) {
                if (!serv.startsWith("org.mpris.MediaPlayer2.")) {
                    continue;
                }
                service = serv;
                break;
            }

            if (service.isEmpty()) {
                QDBusConnectionInterface *dbusDaemonInterface = QDBusConnection::sessionBus().interface();
                connect(dbusDaemonInterface, &QDBusConnectionInterface::serviceOwnerChanged, this,
                        [=](const QString &name, const QString &oldOwner, const QString &newOwner) {
                            Q_UNUSED(oldOwner)
                            if (name.startsWith("org.mpris.MediaPlayer2.") && !newOwner.isEmpty()) {
                                initMediaPlayer();
                                disconnect(dbusDaemonInterface);
                            }
                        });
                return;
            }

            qCDebug(DDE_SHELL) << "Got media player DBus service:" << service;
            initUI();
            auto dbusInter = new DBusMediaPlayer2(service, "/org/mpris/MediaPlayer2", QDBusConnection::sessionBus(), this);
            dbusInter->MetadataChanged();
            dbusInter->PlaybackStatusChanged();
            dbusInter->VolumeChanged();
            dbusInter->deleteLater();
        } else {
            qCWarning(DDE_SHELL) << "Init media player error: " << call.error().message();
        }

        watcher->deleteLater();
    });
}

void MediaWidget::changeVisible()
{
    m_dmprisWidget->setVisible(m_dmprisWidget->isWorking());
}
