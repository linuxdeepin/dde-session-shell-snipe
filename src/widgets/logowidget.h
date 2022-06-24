/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
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
    enum ActiveState {
        Unauthorized = 0,  //未授权
        Authorized,  //已授权
        AuthorizedLapse,  //授权失效
        TrialAuthorized, //试用期已授权
        TrialExpired //试用期已过期
    };

    enum AuthorizationProperty {
        Default = 0,  //默认
        Government,  //政务
        Enterprise  //企业
    };

public:
    LogoWidget(QWidget *parent = nullptr);
    ~LogoWidget() override;

    void updateLocale(const QString &locale);

public Q_SLOTS:
    void updateLicenseState();
    void updateLicenseAuthorizationProperty();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();
    QPixmap loadSystemLogo(const QString &file, bool loadFromDTK);
    QString getVersion();
    void updateLogoPixmap();

private:
    int m_licenseState;
    int m_authorizationProperty;
    QLabel *m_logoLabel;
    DLabel *m_logoVersionLabel;
    QString m_locale;
};
#endif // LOGOFRAME
