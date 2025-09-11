// SPDX-FileCopyrightText: 2021 - 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GESTUREDIALOG_H
#define GESTUREDIALOG_H

#include <DDialog>

DWIDGET_USE_NAMESPACE

class QStackedLayout;
class ResetPatternController;

namespace gestureLogin {
class GesturePannel;
class ModuleWidget;
}

DWIDGET_BEGIN_NAMESPACE
class DTitlebar;
class DPlatformWindowHandle;
DWIDGET_END_NAMESPACE

namespace gestureSetting {

class GestureDialog : public QWidget
{
    Q_OBJECT

public:
    explicit GestureDialog(ResetPatternController *worker, QWidget *parent = nullptr);
    ~GestureDialog() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;     // 过滤鼠标事件

private:
    void initUi();
    void initConnection();
    void createSuccessWidget();
    QWidget* createCancelButton();
    QLabel *createTitleIcon(const QIcon &icon);

private slots:
    void onInputFinished();

private:
    DPlatformWindowHandle *m_handle;
    QWidget *m_titleBar;
    QWidget *m_mainContent;
    QStackedLayout *m_mainLayout;
    gestureLogin::ModuleWidget *m_authWidget;
    QWidget *m_successWidget;
    QPushButton *m_confirmButton;
    ResetPatternController *m_worker;
    QPoint m_dragPosition;
};

class Manager : public QObject
{
    Q_OBJECT

public:
    explicit Manager(int old_file_id, int new_file_id, QObject *parent = nullptr);
    ~Manager() override;

    void start();

    static void exit(int retCode);
    static void free();

private:
    static ResetPatternController *m_controller;
    GestureDialog *m_gestureDialog;
    int m_old_file_id;
    int m_new_file_id;
};

} // namespace gestureSetting

#endif // GESTUREDIALOG_H
