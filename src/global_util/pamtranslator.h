// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PAMTRANSLATOR_H
#define PAMTRANSLATOR_H

#include <QObject>
#include <QMap>

class PamTranslator : public QObject
{
    Q_OBJECT
public:
    static PamTranslator *instance();
    QString trans(const QString &text, bool force = false);
    void setLocal(const QString &);

private:
    explicit PamTranslator(QObject *parent = nullptr);
    PamTranslator(const PamTranslator &) = delete;
    PamTranslator &operator=(const PamTranslator &) = delete;

    QMap<QString, QString> getLocalXmlTranslations();
    QString getTranslation(const QString &content);

private:
    QMap<QString, QString> m_ADomainErrorDetailMap;
    QString m_local;
};

#endif // PAMTRANSLATOR_H
