// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "constants.h"
#include "dbusconstant.h"
#include "fullscreenbackground.h"
#include "sessionbasemodel.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include <atomic>
#include <gtest/gtest.h>

// Runs on its own thread so the client can use a real blocking D-Bus call.
class BackgroundTestService : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    QString result;
    QString scaledResult;
    QString requestedSource;
    int delay = 0;
    bool error = false;
    std::atomic<int> effectCalls{0};
    std::atomic<int> scaleCalls{0};

public Q_SLOTS:
    QString Get(const QString &, const QString &source)
    {
        requestedSource = source;
        ++effectCalls;
        if (error) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("Test generation failure"));
            return {};
        }
        if (delay > 0) {
            setDelayedReply(true);
            const auto request = message();
            const auto bus = connection();
            const auto path = result;
            QTimer::singleShot(delay, Qt::PreciseTimer, this, [request, bus, path] {
                bus.send(request.createReply(QVariantList{path}));
            });
            return {};
        }
        return result;
    }

    QStringList GetProcessedImagePathByFd(const QDBusUnixFileDescriptor &, const QString &, const QVariantList &)
    {
        ++scaleCalls;
        return scaledResult.isEmpty() ? QStringList{} : QStringList{scaledResult};
    }
};

class UT_BackgroundFallback : public testing::Test
{
protected:
    void SetUp() override
    {
        // Never register fake system services on a desktop's actual system bus.
        if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS").isEmpty()
            || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS")) {
            GTEST_SKIP() << "Run with tests/dde-lock/run-background-tests.sh";
        }

        ASSERT_TRUE(directory.isValid());
        source = directory.filePath("source.png");
        effect = directory.filePath("effect.png");
        QImage image(128, 96, QImage::Format_RGB32);
        image.fill(Qt::red);
        ASSERT_TRUE(image.save(source));
        image.fill(Qt::blue);
        ASSERT_TRUE(image.save(effect));

        service = new BackgroundTestService;
        service->result = effect;
        service->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, service, &QObject::deleteLater);
        thread.start();
        bool registered = false;
        QMetaObject::invokeMethod(service, [&] {
            auto bus = QDBusConnection::connectToBus(
                QString::fromLocal8Bit(qgetenv("DBUS_SYSTEM_BUS_ADDRESS")), connectionName);
            registered = bus.registerService(DSS_DBUS::imageEffectService)
                && bus.registerObject(DSS_DBUS::imageEffectPath, DSS_DBUS::imageEffectService,
                                      service, QDBusConnection::ExportAllSlots)
                && bus.registerService("org.deepin.dde.WallpaperCache")
                && bus.registerObject("/org/deepin/dde/WallpaperCache", "org.deepin.dde.WallpaperCache",
                                      service, QDBusConnection::ExportAllSlots);
        }, Qt::BlockingQueuedConnection);
        ASSERT_TRUE(registered);

        model = new SessionBaseModel;
        user = std::make_shared<User>();
        user->m_greeterBackground = source;
        model->updateCurrentUser(user);
        background = new FullScreenBackground(model);
        background->resize(64, 48);
        content = new QWidget;
        FullScreenBackground::currentContent = content;
    }

    void TearDown() override
    {
        delete background;
        delete content;
        delete model;
        if (service) {
            QMetaObject::invokeMethod(service, [&] {
                auto bus = QDBusConnection(connectionName);
                bus.unregisterObject(DSS_DBUS::imageEffectPath);
                bus.unregisterObject("/org/deepin/dde/WallpaperCache");
                bus.unregisterService(DSS_DBUS::imageEffectService);
                bus.unregisterService("org.deepin.dde.WallpaperCache");
                QDBusConnection::disconnectFromBus(connectionName);
            }, Qt::BlockingQueuedConnection);
            thread.quit();
            thread.wait();
        }
    }

    void configure(const QString &result, int delay = 0, bool error = false, const QString &scaled = {})
    {
        QMetaObject::invokeMethod(service, [&] {
            service->result = result;
            service->delay = delay;
            service->error = error;
            service->scaledResult = scaled;
        }, Qt::BlockingQueuedConnection);
    }

    void showBackground()
    {
        // Match the application: background preparation is connected first,
        // then the model's visibility signal shows the window.
        QObject::connect(model, &SessionBaseModel::visibleChanged, background, &QWidget::setVisible);
        model->setVisible(true);
    }

    static bool scalingSupported()
    {
#ifndef ENABLE_DSS_SNIPE
        // Qt 5 checks the service unit before contacting even our mock service.
        return QFile::exists(QStringLiteral("/lib/systemd/system/dde-wallpaper-cache.service"));
#else
        return true;
#endif
    }

    QString requestedSource()
    {
        QString source;
        QMetaObject::invokeMethod(service, [&] {
            source = service->requestedSource;
        }, Qt::BlockingQueuedConnection);
        return source;
    }

    void expectSolid(FullScreenBackground *frame)
    {
        EXPECT_TRUE(frame->blurBackgroundPath.isEmpty());
        EXPECT_TRUE(frame->blurBackgroundCacheMap.isEmpty());
        const auto image = frame->grab().toImage();
        ASSERT_FALSE(image.isNull());
        EXPECT_EQ(image.pixelColor(image.width() / 2, image.height() / 2),
                  QColor(DDESESSIONCC::SOLID_BACKGROUND_COLOR));
    }

    const QString connectionName = QStringLiteral("background-regression-test");
    QTemporaryDir directory;
    QPointer<QWidget> content;
    QThread thread;
    BackgroundTestService *service = nullptr;
    SessionBaseModel *model = nullptr;
    FullScreenBackground *background = nullptr;
    std::shared_ptr<User> user;
    QString source;
    QString effect;
};

TEST_F(UT_BackgroundFallback, SuccessUsesEffectAndReusesSizeCache)
{
    background->updateBackground(source);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(service->scaleCalls, scalingSupported() ? 1 : 0);
    const int scaleCalls = service->scaleCalls;

    background->updateBackground(source);
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(service->scaleCalls, scaleCalls);

    FullScreenBackground second(model);
    second.resize(64, 48);
    second.updateBackground(source);
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(service->scaleCalls, scaleCalls);
    EXPECT_EQ(second.grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, TimeoutStaysSolidUntilNextDisplay)
{
    configure(effect, 6000);
    QElapsedTimer elapsed;
    elapsed.start();
    background->updateBackground(source);
    EXPECT_GE(elapsed.elapsed(), 4500);
    // Leave room for scheduler delays on loaded CI workers.
    EXPECT_LT(elapsed.elapsed(), 9000);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(service->scaleCalls, 0);

    // First visibility notification must not repeat the pre-show timeout.
    model->setVisible(true);
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);

    // The service is now ready, but this display still keeps its solid fallback.
    configure(effect);
    QTest::qWait(1500);
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);

    model->setVisible(false);
    model->setVisible(true);
    EXPECT_EQ(service->effectCalls, 2);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, SlowSuccessIsReadyBeforeFirstShow)
{
    configure(effect, 4000);
    QElapsedTimer elapsed;
    elapsed.start();
    background->updateBackground(source);
    EXPECT_GE(elapsed.elapsed(), 3900);
    ASSERT_EQ(background->blurBackgroundPath, effect);
    EXPECT_FALSE(background->isVisible());
    showBackground();
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, ShowingPreservesRequestedWorkspaceWallpaper)
{
    const QString workspace = directory.filePath("workspace.png");
    ASSERT_TRUE(QFile::copy(source, workspace));
    ASSERT_NE(workspace, user->greeterBackground());
    background->updateBackground(workspace);
    ASSERT_EQ(background->blurBackgroundPath, effect);
    configure({}, 0, true);

    model->setVisible(true);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    model->setVisible(false);
    model->setVisible(true);

    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(requestedSource(), workspace);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, NextDisplayRetriesRequestedWorkspaceWallpaper)
{
    FullScreenBackground second(model);
    second.resize(80, 60);
    const QString workspace = directory.filePath("workspace.png");
    ASSERT_TRUE(QFile::copy(source, workspace));
    ASSERT_NE(workspace, user->greeterBackground());
    configure({}, 0, true);
    background->updateBackground(workspace);
    model->setVisible(true);
    expectSolid(background);
    expectSolid(&second);
    EXPECT_EQ(service->effectCalls, 1);

    model->setVisible(false);
    configure(effect);
    model->setVisible(true);

    EXPECT_EQ(service->effectCalls, 2);
    EXPECT_EQ(requestedSource(), workspace);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    EXPECT_EQ(second.grab().toImage().pixelColor(40, 30), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, FailureClearsAllScreenSizes)
{
    FullScreenBackground second(model);
    second.resize(80, 60);
    background->updateBackground(source);
    ASSERT_FALSE(background->blurBackgroundCacheMap.isEmpty());

    const QString other = directory.filePath("other.png");
    ASSERT_TRUE(QFile::copy(source, other));
    configure({}, 0, true);
    background->updateBackground(other);
    expectSolid(background);
    expectSolid(&second);
    const int calls = service->effectCalls;
    const int scaleCalls = service->scaleCalls;

    second.updateBackground(other);
    second.resize(96, 72);
    expectSolid(&second);
    EXPECT_EQ(service->effectCalls, calls);
    EXPECT_EQ(service->scaleCalls, scaleCalls);
}

TEST_F(UT_BackgroundFallback, NewBackgroundReplacesAllScreenSizes)
{
    FullScreenBackground second(model);
    second.resize(80, 60);
    background->updateBackground(source);

    const QString next = directory.filePath("next.png");
    const QString nextEffect = directory.filePath("next-effect.png");
    ASSERT_TRUE(QFile::copy(source, next));
    QImage image(128, 96, QImage::Format_RGB32);
    image.fill(Qt::green);
    ASSERT_TRUE(image.save(nextEffect));
    configure(nextEffect);
    background->updateBackground(next);

    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::green));
    EXPECT_EQ(second.grab().toImage().pixelColor(40, 30), QColor(Qt::green));
}

TEST_F(UT_BackgroundFallback, InvalidInputClearsPreviouslyLoadedBackground)
{
    background->updateBackground(source);
    ASSERT_FALSE(background->blurBackgroundCacheMap.isEmpty());
    const int calls = service->effectCalls;
    background->updateBackground(directory.filePath("missing.png"));
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, calls);
}

TEST_F(UT_BackgroundFallback, EmptyOrInvalidServiceResultIsNotCached)
{
    configure({});
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->scaleCalls, 0);

    model->setVisible(true);
    model->setVisible(false);
    configure(directory.filePath("missing-effect.png"));
    model->setVisible(true);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 2);
    EXPECT_EQ(service->scaleCalls, 0);
}

TEST_F(UT_BackgroundFallback, CorruptScaledCacheFallsBackToEffect)
{
    if (!scalingSupported())
        GTEST_SKIP() << "The Qt 5 build requires the optional wallpaper-cache service unit";

    const QString corrupt = directory.filePath("broken_64x48.png");
    QFile file(corrupt);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("broken image");
    file.close();
    configure(effect, 0, false, corrupt);

    background->updateBackground(source);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, DecodeFailureFallsBackToSolid)
{
    const QString truncated = directory.filePath("truncated.png");
    QFile original(effect);
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    QFile file(truncated);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(original.read(33)), 33);
    file.close();
    // A readable PNG header alone does not guarantee that pixels can be decoded.
    ASSERT_TRUE(background->isPicture(truncated));
    configure(truncated);

    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);
    background->updateBackground(source);
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, RemovedEffectInvalidatesSuccessfulCache)
{
    background->updateBackground(source);
    ASSERT_FALSE(background->blurBackgroundCacheMap.isEmpty());
    ASSERT_TRUE(QFile::remove(effect));
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, ScreenRemovalDoesNotResetFailedAttempt)
{
    configure({}, 0, true);
    {
        FullScreenBackground second(model);
        second.updateBackground(source);
    }
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, EmptyScreenSizeDoesNotInvalidateOtherScreens)
{
    FullScreenBackground second(model);
    second.resize(0, 0);
    background->updateBackground(source);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));

    second.resize(80, 60);
    EXPECT_EQ(second.grab().toImage().pixelColor(40, 30), QColor(Qt::blue));
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, OlderReplyCannotReplaceNewBackground)
{
    const QString next = directory.filePath("next.png");
    const QString nextEffect = directory.filePath("next-effect.png");
    ASSERT_TRUE(QFile::copy(source, next));
    QImage image(128, 96, QImage::Format_RGB32);
    image.fill(Qt::green);
    ASSERT_TRUE(image.save(nextEffect));
    showBackground();
    configure(effect, 300);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 1; }, 1000));
    configure(nextEffect);
    background->updateBackground(next);
    ASSERT_TRUE(QTest::qWaitFor([&] { return background->blurBackgroundPath == nextEffect; }, 1000));
    QTest::qWait(350);
    EXPECT_EQ(service->effectCalls, 2);
    EXPECT_EQ(background->blurBackgroundPath, nextEffect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::green));
}

TEST_F(UT_BackgroundFallback, RemovedRequesterDoesNotAbandonOtherScreens)
{
    FullScreenBackground second(model);
    second.resize(80, 60);
    showBackground();
    configure(effect, 200);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 1; }, 1000));
    delete background;
    background = nullptr;
    expectSolid(&second);
    second.updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return second.blurBackgroundPath == effect; }, 1000));
    EXPECT_EQ(second.grab().toImage().pixelColor(40, 30), QColor(Qt::blue));
    EXPECT_EQ(service->effectCalls, 1);
}

TEST_F(UT_BackgroundFallback, VisibleUpdateReturnsBeforeReplyAndSharesPendingRequest)
{
    showBackground();
    configure(effect, 400);
    QElapsedTimer elapsed;
    elapsed.start();
    background->updateBackground(source);
    EXPECT_LT(elapsed.elapsed(), 200);
    expectSolid(background);

    FullScreenBackground second(model);
    second.resize(80, 60);
    second.updateBackground(source);
    background->updateBackground(source);
    bool timerRan = false;
    QTimer::singleShot(0, background, [&] { timerRan = true; });
    ASSERT_TRUE(QTest::qWaitFor([&] { return timerRan && service->effectCalls == 1; }, 200));
    expectSolid(background);
    ASSERT_TRUE(QTest::qWaitFor([&] { return background->blurBackgroundPath == effect; }, 1500));
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    EXPECT_EQ(second.grab().toImage().pixelColor(40, 30), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, HiddenScreenUsesAsyncWhileAnotherScreenIsShown)
{
    showBackground();
    FullScreenBackground second(model);
    second.resize(80, 60);
    ASSERT_FALSE(second.isVisible());
    configure(effect, 400);
    QElapsedTimer elapsed;
    elapsed.start();
    second.updateBackground(source);
    EXPECT_LT(elapsed.elapsed(), 200);
    expectSolid(&second);
    ASSERT_TRUE(QTest::qWaitFor([&] { return second.blurBackgroundPath == effect; }, 1500));
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
}

TEST_F(UT_BackgroundFallback, RepeatedPathCannotAcceptAnOlderRequest)
{
    const QString other = directory.filePath("other.png");
    const QString nextEffect = directory.filePath("next-effect.png");
    ASSERT_TRUE(QFile::copy(source, other));
    QImage image(128, 96, QImage::Format_RGB32);
    image.fill(Qt::green);
    ASSERT_TRUE(image.save(nextEffect));
    showBackground();

    configure(effect, 300);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 1; }, 1000));
    background->updateBackground(other);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 2; }, 1000));
    configure(nextEffect);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return background->blurBackgroundPath == nextEffect; }, 1000));
    QTest::qWait(350);
    EXPECT_EQ(service->effectCalls, 3);
    EXPECT_EQ(background->blurBackgroundPath, nextEffect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::green));
}

TEST_F(UT_BackgroundFallback, HidingDiscardsPendingReplyAndRetriesBeforeShowing)
{
    showBackground();
    configure(effect, 200);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 1; }, 1000));
    model->setVisible(false);
    QTest::qWait(250);
    expectSolid(background);

    configure(effect, 150);
    model->setVisible(true);
    // No event-loop wait: the retry must already be complete at first show.
    EXPECT_TRUE(background->isVisible());
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(background->grab().toImage().pixelColor(32, 24), QColor(Qt::blue));
    EXPECT_EQ(service->effectCalls, 2);
}

TEST_F(UT_BackgroundFallback, LastScreenRemovalInvalidatesPendingReply)
{
    showBackground();
    configure(effect, 200);
    background->updateBackground(source);
    ASSERT_TRUE(QTest::qWaitFor([&] { return service->effectCalls == 1; }, 1000));
    delete background;
    background = new FullScreenBackground(model);
    background->resize(64, 48);
    QTest::qWait(250);
    expectSolid(background);

    configure(effect);
    background->updateBackground(source);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(service->effectCalls, 2);
}

TEST_F(UT_BackgroundFallback, AsyncTimeoutStaysSolidUntilNextDisplay)
{
    showBackground();
    configure(effect, 6000);
    QElapsedTimer elapsed;
    elapsed.start();
    background->updateBackground(source);
    EXPECT_LT(elapsed.elapsed(), 500);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return background->backgroundState == FullScreenBackground::BackgroundState::Failed;
    }, 9000));
    QTest::qWait(1500);
    background->updateBackground(source);
    expectSolid(background);
    EXPECT_EQ(service->effectCalls, 1);
    EXPECT_EQ(service->scaleCalls, 0);

    model->setVisible(false);
    configure(effect);
    model->setVisible(true);
    EXPECT_EQ(background->blurBackgroundPath, effect);
    EXPECT_EQ(service->effectCalls, 2);
}

#include "ut_backgroundfallback.moc"
