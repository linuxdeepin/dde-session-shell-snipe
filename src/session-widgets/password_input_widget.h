// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PASSWORD_INPUT_WIDGET_H
#define PASSWORD_INPUT_WIDGET_H

#include <DLineEdit>
#include <DLabel>

#include <QLineEdit>
#include <DIconButton>

DWIDGET_USE_NAMESPACE

/**
 * @brief 带密码显示按钮的密码输入控件
 *
 * 继承自DLineEdit，在内部添加密码显示/隐藏按钮和capslock状态显示
 * 自动监听键盘capslock状态变化，无需外部手动设置
 * 保持DLineEdit的所有接口兼容性，最小化代码改动
 */
class PasswordInputWidget : public DLineEdit
{
    Q_OBJECT

public:
    explicit PasswordInputWidget(QWidget *parent = nullptr);
    ~PasswordInputWidget() override = default;

    // 密码显示按钮控制
    void setPasswordShowButtonVisible(bool visible);
    bool isPasswordShowButtonVisible() const;

    // 支持长按显示密码模式
    void setLongPressMode(bool enabled);
    bool isLongPressMode() const;

    // CapsLock状态控制（主要用于手动控制，通常由内部自动处理）
    void setCapsLockVisible(bool visible);
    bool isCapsLockVisible() const;

    // 获取密码显示按钮（用于特殊需求）
    DIconButton *passwordShowButton() const { return m_passwordShowBtn; }

    // 获取CapsLock状态标签（用于特殊需求）
    DLabel *capsLockLabel() const { return m_capsLockLabel; }

    // 便利方法
    void selectAll();
    // override QLineEdit::clear()：使用 setText("") 获得 Qt 文档承诺的 undo/redo 清栈行为
    void clear();

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onPasswordShowPressed();
    void onPasswordShowReleased();
    void onPasswordShowClicked();
    void onTextChanged(const QString &text);
    void onCapsLockStatusChanged(bool on);
    void clearUndoRedoStack();

private:
    void initPasswordButton();
    void initCapsLockLabel();
    void initKeyboardMonitor();
    void initConnections();
    void updatePasswordButtonConnections();
    void updatePasswordButtonLayout();
    void updateCapsLockLayout();
    void updatePasswordVisibility(bool visible);
    void updatePasswordTextMargins();

private:
    DIconButton *m_passwordShowBtn; // 密码显示按钮
    DLabel *m_capsLockLabel; // CapsLock状态标签
    bool m_longPressMode; // 是否启用长按模式
    bool m_passwordVisible; // 当前密码是否可见
};

#endif // PASSWORD_INPUT_WIDGET_H
