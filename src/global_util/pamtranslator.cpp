// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pamtranslator.h"
#include "public_func.h"
#include "dconfig_helper.h"

#include <QLocale>
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>

const QString serverTitle = "Server message:";
const QString serverTitle2 = "BAD PASSWORD:";

PamTranslator *PamTranslator::instance()
{
    static PamTranslator pt;
    return &pt;
}

PamTranslator::PamTranslator(QObject *parent)
    : QObject(parent)
    , m_local(QLocale::system().name())
{
    m_ADomainErrorDetailMap = getLocalXmlTranslations();
}

QString PamTranslator::trans(const QString &errorTxt, bool force)
{
    if (m_ADomainErrorDetailMap.isEmpty()) {
        return errorTxt;
    }

    QString content = getTranslation(errorTxt);

    // 如果force为true,没有对应方案时用complexity constraints的翻译作为结果
    if (force) {
        if (!m_ADomainErrorDetailMap.values().contains(content)) {
            content = getTranslation("complexity constraints");
            qInfo() << Q_FUNC_INFO << " Content lost in detailMap : " << content;
        }
    }

    return content;
}

void PamTranslator::setLocal(const QString &localName)
{
    if (m_local == localName) {
        return;
    }

    qCDebug(DDE_SHELL) << "switch to local : " << m_local;

    m_local = localName;
    m_ADomainErrorDetailMap.clear();
    m_ADomainErrorDetailMap = getLocalXmlTranslations();
}

QMap<QString, QString> PamTranslator::getLocalXmlTranslations()
{
    QMap<QString, QString> translationsMap;

    auto tsPath = DConfigHelper::instance()->getConfig("pamTranslationTSPrefix", "").toString();
    if (tsPath.isEmpty()) {
        return translationsMap;
    }

    QFile file(tsPath + m_local + ".ts");
    qDebug() << "getLocalXmlTranslations xmlFilePath : " << file.fileName();
    if (!file.exists()) {
        qCWarning(DDE_SHELL) << "File not exist : " << file.fileName();
        return translationsMap;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(DDE_SHELL) << "Open file failed : " << file.fileName();
        return translationsMap;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();

        if (xml.isStartElement() && xml.name() == "message") {
            QString source;
            QString translation;

            while (xml.readNextStartElement()) {
                if (xml.name() == "source") {
                    source = xml.readElementText();
                } else if (xml.name() == "translation") {
                    translation = xml.readElementText();
                }
            }

            if (!source.isEmpty() && !translation.isEmpty()) {
                translationsMap[source] = translation;
            }
        }
    }
    file.close();

    return translationsMap;
}

QString PamTranslator::getTranslation(const QString &content)
{
    QString data = content.toLower();

    // 优先严格匹配
    auto source = content.trimmed().toLower();
    source = source.replace(":", "");
    if (m_ADomainErrorDetailMap.contains(content)) {
        return m_ADomainErrorDetailMap.value(content);
    }

    auto keys = m_ADomainErrorDetailMap.keys();

    QStringList lines = data.split("\n", Qt::SkipEmptyParts);
    QString tmpResutl = "";
    QString result = "";

    for (auto line : lines) {
        auto currentLine = line;
        currentLine = currentLine.replace(serverTitle, "");
        currentLine = currentLine.replace(serverTitle2, "");
        currentLine = currentLine.trimmed();

        if (keys.contains(currentLine)) {
            return m_ADomainErrorDetailMap.value(currentLine);
        } else {
            // 取最长内容翻译
            int matchDelta = INT_MAX;
            for (auto key : keys) {
                if (currentLine.contains(key.toLower())) {
                    tmpResutl = m_ADomainErrorDetailMap.value(key);
                    if (!tmpResutl.isEmpty()) {
                        auto delta = qAbs(currentLine.length() - key.length());
                        if (delta < matchDelta) {
                            result = tmpResutl;
                            matchDelta = delta;
                        }
                    }
                }
            }
        }
    }

    // 尝试取最后一个匹配项，避免翻译冲突
    if (!result.isEmpty()) {
        return result;
    }

    qWarning() << "No translation found for PAM message";
    return content;
}
