// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGOWIDGET
#define LOGOWIDGET

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <DLabel>

DWIDGET_USE_NAMESPACE

class LogoWidget : public QFrame
{
    Q_OBJECT
public:
    LogoWidget(QWidget *parent = nullptr);
    ~LogoWidget() override;

    void updateLocale(const QString &locale);
    static void onDConfigPropertyChanged(const QString &key, const QVariant &value, QObject* objPtr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();
    QPixmap loadSystemLogo();
    void loadCustomLogo();
    QString getVersion();
    void updateCustomLogoPos();

private:
    QLabel *m_logoLabel;
    DLabel *m_logoVersionLabel;
    QString m_locale;

    // 第三方logo配置控制显示
    QLabel *m_customLogoLabel;
};
#endif // LOGOFRAME
