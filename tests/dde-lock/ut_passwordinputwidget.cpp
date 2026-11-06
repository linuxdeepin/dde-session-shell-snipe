// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockpasswordchangewidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>
#include <memory>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformclipboard.h>
#include <qpa/qplatformintegration.h>

namespace {
class SelectionClipboard : public QPlatformClipboard
{
public:
    QMimeData *mimeData(QClipboard::Mode mode) override
    {
        return mode == QClipboard::Selection ? selection.get() : clipboard.get();
    }
    void setMimeData(QMimeData *data, QClipboard::Mode mode) override
    {
        (mode == QClipboard::Selection ? selection : clipboard).reset(data);
        emitChanged(mode);
    }
    bool supportsMode(QClipboard::Mode mode) const override
    {
        return mode == QClipboard::Clipboard || mode == QClipboard::Selection;
    }
    bool ownsMode(QClipboard::Mode mode) const override { return supportsMode(mode); }

private:
    std::unique_ptr<QMimeData> selection;
    std::unique_ptr<QMimeData> clipboard;
};

// The offscreen plugin has no PRIMARY selection. Replace only clipboard
// behavior during this synchronous probe and delegate GUI operations to it.
class SelectionIntegration : public QPlatformIntegration
{
public:
    explicit SelectionIntegration(QPlatformIntegration *original) : original(original) {}
    QPlatformClipboard *clipboard() const override { return &selection; }
    QPlatformWindow *createPlatformWindow(QWindow *window) const override { return original->createPlatformWindow(window); }
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override { return original->createPlatformBackingStore(window); }
    QAbstractEventDispatcher *createEventDispatcher() const override { return original->createEventDispatcher(); }
    bool hasCapability(Capability capability) const override { return original->hasCapability(capability); }
    QPlatformFontDatabase *fontDatabase() const override { return original->fontDatabase(); }
    QPlatformInputContext *inputContext() const override { return original->inputContext(); }
    QPlatformNativeInterface *nativeInterface() const override { return original->nativeInterface(); }
    QVariant styleHint(StyleHint hint) const override { return original->styleHint(hint); }

private:
    QPlatformIntegration *original;
    mutable SelectionClipboard selection;
};
}

TEST(UT_PasswordInputWidget, CompleteMiddleClickCannotPastePrimaryIntoAnyPasswordField)
{
    if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
        || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
        GTEST_SKIP() << "Requires the isolated test bus";

    LockPasswordChangeWidget page;
    page.m_localSecurityCheck = false;
    page.setState(true, false, false, {});
    page.setFixedWidth(360);
    page.show();
    QLineEdit ordinary;
    ordinary.resize(240, 30);
    ordinary.show();
    SelectionIntegration integration(QGuiApplicationPrivate::platformIntegration());
    QScopedValueRollback<QPlatformIntegration *> restore(QGuiApplicationPrivate::platform_integration, &integration);
    auto *clipboard = QApplication::clipboard();
    ASSERT_TRUE(clipboard->supportsSelection());
    const auto primary = QStringLiteral("selection-value");

    // Positive control: a complete click must paste with this clipboard. The
    // regression would otherwise pass vacuously on an offscreen platform.
    clipboard->setText(primary, QClipboard::Selection);
    QTest::mouseClick(&ordinary, Qt::MiddleButton);
    ASSERT_EQ(ordinary.text(), primary);

    for (auto *field : {page.m_current, page.m_password, page.m_confirmation}) {
        EXPECT_FALSE(field->pasteEnabled());
        for (const auto mode : {QLineEdit::Password, QLineEdit::Normal}) {
            field->setEchoMode(mode);
            field->setText(QStringLiteral("typed-secret"));
            field->lineEdit()->selectAll();
            clipboard->setText(primary, QClipboard::Selection);
            QTest::mouseClick(field->lineEdit(), Qt::MiddleButton);
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret"));
            // A release can arrive without its press after a UI transfer.
            QTest::mouseRelease(field->lineEdit(), Qt::MiddleButton);
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret"));
            clipboard->setText(QStringLiteral("clipboard-value"));
            QTest::keyClick(field->lineEdit(), Qt::Key_V, Qt::ControlModifier);
            QTest::keyClick(field->lineEdit(), Qt::Key_Insert, Qt::ShiftModifier);
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret"));
            // Normal keyboard input remains usable.
            field->lineEdit()->deselect();
            field->lineEdit()->setCursorPosition(field->text().size());
            QTest::keyClicks(field->lineEdit(), "!");
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret!"));
        }
    }
}

TEST(UT_PasswordInputWidget, CompleteTextDropCannotInsertIntoAnyPasswordField)
{
    if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
        || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
        GTEST_SKIP() << "Requires the isolated test bus";

    QMimeData mime;
    mime.setText(QStringLiteral("dropped-value"));
    const auto dropText = [&mime](QLineEdit *edit) {
        QDragEnterEvent enter(QPoint(5, 5), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(edit, &enter);
        if (!enter.isAccepted())
            return false;
        QDragMoveEvent move(QPoint(5, 5), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(edit, &move);
        QDropEvent drop(QPointF(5, 5), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(edit, &drop);
        return drop.isAccepted();
    };
    QLineEdit ordinary;
    ordinary.resize(240, 30);
    ordinary.show();
    ASSERT_TRUE(dropText(&ordinary));
    ASSERT_EQ(ordinary.text(), mime.text());

    LockPasswordChangeWidget page;
    page.m_localSecurityCheck = false;
    page.setState(true, false, false, {});
    page.setFixedWidth(360);
    page.show();
    for (auto *field : {page.m_current, page.m_password, page.m_confirmation}) {
        EXPECT_FALSE(field->lineEdit()->acceptDrops());
        for (const auto mode : {QLineEdit::Password, QLineEdit::Normal}) {
            field->setEchoMode(mode);
            field->setText(QStringLiteral("typed-secret"));
            EXPECT_FALSE(dropText(field->lineEdit()));
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret"));
            field->lineEdit()->setCursorPosition(field->text().size());
            QTest::keyClicks(field->lineEdit(), "!");
            EXPECT_EQ(field->text(), QStringLiteral("typed-secret!"));
        }
    }
}

TEST(UT_PasswordInputWidget, RevealedPasswordsNeverPublishPrimarySelection)
{
    if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
        || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
        GTEST_SKIP() << "Requires the isolated test bus";

    LockPasswordChangeWidget page;
    page.m_localSecurityCheck = false;
    page.setState(true, false, false, {});
    page.setFixedWidth(360);
    page.show();
    QLineEdit ordinary;
    ordinary.resize(240, 30);
    ordinary.show();
    SelectionIntegration integration(QGuiApplicationPrivate::platformIntegration());
    QScopedValueRollback<QPlatformIntegration *> restore(QGuiApplicationPrivate::platform_integration, &integration);
    auto *clipboard = QApplication::clipboard();
    ASSERT_TRUE(clipboard->supportsSelection());
    QSignalSpy selectionWrites(clipboard, &QClipboard::selectionChanged);

    // Verify actual automatic export, not just final clipboard contents: a
    // secret published and then cleared could already reach a clipboard manager.
    ordinary.setText(QStringLiteral("ordinary-selection"));
    QTest::keyClick(&ordinary, Qt::Key_A, Qt::ControlModifier);
    ASSERT_EQ(clipboard->text(QClipboard::Selection), ordinary.text());
    ASSERT_GT(selectionWrites.count(), 0);
    ordinary.deselect();
    QTest::mousePress(&ordinary, Qt::LeftButton, Qt::NoModifier, QPoint(1, ordinary.height() / 2));
    QTest::mouseMove(&ordinary, ordinary.rect().center());
    QTest::mouseMove(&ordinary, QPoint(ordinary.width() - 1, ordinary.height() / 2));
    ASSERT_EQ(ordinary.selectedText(), ordinary.text());
    QTest::mouseRelease(&ordinary, Qt::LeftButton, Qt::NoModifier, QPoint(ordinary.width() - 1, ordinary.height() / 2));
    ASSERT_EQ(clipboard->text(QClipboard::Selection), ordinary.text());
    const auto previousSelection = QStringLiteral("unrelated-selection");
    const auto secret = QStringLiteral("TypedSecret9!");

    for (auto *field : {page.m_current, page.m_password, page.m_confirmation}) {
        for (bool longPress : {true, false}) {
            SCOPED_TRACE(longPress);
            field->setLongPressMode(longPress);
            field->setText(secret);
            auto *edit = field->lineEdit();
            edit->setFocus();
            clipboard->setText(previousSelection, QClipboard::Selection);
            selectionWrites.clear();
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            EXPECT_EQ(edit->selectedText(), secret);
            EXPECT_EQ(selectionWrites.count(), 0);
            edit->deselect();
            if (longPress)
                QTest::mousePress(field->passwordShowButton(), Qt::LeftButton);
            else
                QTest::mouseClick(field->passwordShowButton(), Qt::LeftButton);
            ASSERT_EQ(edit->displayText(), secret);

            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            EXPECT_EQ(edit->selectedText(), secret);
            EXPECT_EQ(selectionWrites.count(), 0);
            QTest::keyClick(edit, Qt::Key_C, Qt::ControlModifier);
            QTest::keyClick(edit, Qt::Key_Home);
            QTest::keyClick(edit, Qt::Key_End, Qt::ShiftModifier);
            EXPECT_EQ(edit->selectedText(), secret);
            EXPECT_EQ(selectionWrites.count(), 0);

            // Mouse selection is available in toggle mode; long-press mode
            // keeps the left button held on the reveal button itself.
            if (!longPress) {
                edit->deselect();
                const QPoint start(1, edit->height() / 2);
                const QPoint end(edit->width() - 1, edit->height() / 2);
                QTest::mousePress(edit, Qt::LeftButton, Qt::NoModifier, start);
                EXPECT_EQ(edit->cursorPosition(), 0);
                // Use a drag sequence: the first move can initialize tracking
                // before subsequent moves extend the text selection.
                QTest::mouseMove(edit, edit->rect().center());
                QTest::mouseMove(edit, end);
                EXPECT_EQ(edit->selectedText(), secret);
                QTest::mouseRelease(edit, Qt::LeftButton, Qt::NoModifier, end);
                EXPECT_EQ(edit->selectedText(), secret);
                EXPECT_EQ(selectionWrites.count(), 0);
                edit->deselect();
                QTest::mouseDClick(edit, Qt::LeftButton);
                QTest::mouseRelease(edit, Qt::LeftButton);
                EXPECT_FALSE(edit->selectedText().isEmpty());
                EXPECT_EQ(edit->displayText(), secret);
                EXPECT_EQ(selectionWrites.count(), 0);
            }

            // Selecting and replacing text remains usable while revealed.
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(edit, "replacement");
            EXPECT_EQ(edit->text(), QStringLiteral("replacement"));
            EXPECT_EQ(edit->displayText(), edit->text());
            if (longPress)
                QTest::mouseRelease(field->passwordShowButton(), Qt::LeftButton);
            else
                QTest::mouseClick(field->passwordShowButton(), Qt::LeftButton);
            field->clear();
            EXPECT_EQ(field->echoMode(), QLineEdit::Password);
            EXPECT_EQ(clipboard->text(QClipboard::Selection), previousSelection);
            EXPECT_EQ(selectionWrites.count(), 0);
        }
    }
}

TEST(UT_PasswordInputWidget, RevealedSubmissionDoesNotRestoreVisibilityAfterClearing)
{
    if (qgetenv("DSS_TEST_PRIVATE_BUS") != "1"
        || qgetenv("DBUS_SYSTEM_BUS_ADDRESS") != qgetenv("DBUS_SESSION_BUS_ADDRESS"))
        GTEST_SKIP() << "Requires the isolated test bus";

    PasswordInputWidget field;
    field.setLongPressMode(false);
    field.setText(QStringLiteral("submitted-secret"));
    field.show();
    QObject::connect(&field, &DLineEdit::returnPressed, &field, &PasswordInputWidget::clear);
    QSignalSpy submissions(&field, &DLineEdit::returnPressed);
    QTest::mouseClick(field.passwordShowButton(), Qt::LeftButton);
    ASSERT_EQ(field.echoMode(), QLineEdit::Normal);
    QTest::keyClick(field.lineEdit(), Qt::Key_Return);
    EXPECT_EQ(submissions.count(), 1);
    EXPECT_TRUE(field.text().isEmpty());
    EXPECT_FALSE(field.m_passwordVisible);
    EXPECT_EQ(field.echoMode(), QLineEdit::Password);
}
