// SPDX-FileCopyrightText: 2015 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xkbparser.h"
#include "constants.h"

#include <QDomDocument>
#include <QDebug>

#include <locale.h>
#include <libintl.h>

XkbParser::XkbParser(QObject *parent)
    : QObject(parent)
{
}

QStringList XkbParser::lookUpKeyboardList(QStringList keyboardList_key) {
    if (KeyboardLayoutList.isEmpty()) {
        parse();
    }

    QStringList result;
    setlocale(LC_ALL, "");
    const char xkbDomain[] = "xkeyboard-config";

    for (int k = 0; k < keyboardList_key.length(); k++) {
        QString tmpKey, head_key, tail_key;
        tmpKey = keyboardList_key[k];
        QStringList tmpKeyList = tmpKey.split(";");
        if (tmpKeyList.length() == 2) {
            head_key = tmpKeyList[0];
            tail_key = tmpKeyList[1];

            for (int i = 0; i < KeyboardLayoutList.length(); i++) {
                if (KeyboardLayoutList[i].name == head_key) {
                    if (tail_key.isEmpty()) {
                        result << dgettext(xkbDomain, KeyboardLayoutList[i].description.toLatin1());
                        break;
                    } else {
                        for (int j = 0; j < KeyboardLayoutList[i].variantItemList.length(); j++) {
                            if (KeyboardLayoutList[i].variantItemList[j].name == tail_key) {
                                result << dgettext(xkbDomain, KeyboardLayoutList[i].variantItemList[j].description.toLatin1());
                            }
                        }
                        break;
                    }
                }
            }
        } else {
            continue;
        }
    }

    return result;
}
QString XkbParser::lookUpKeyboardKey(const QString &keyboard_value)
{
    if (KeyboardLayoutList.isEmpty()) {
        parse();
    }

    QString keyboard_key = "";
    for (int i = 0; i < KeyboardLayoutList.length(); i++) {
        if (KeyboardLayoutList[i].description == keyboard_value) {
            keyboard_key = QString("%1|").arg(KeyboardLayoutList[i].name);
            qDebug() << "Look up keyboard key:" << keyboard_key;
            return keyboard_key;
        } else {
            for (int j = 0; j < KeyboardLayoutList[i].variantItemList.length(); j++) {
                if (KeyboardLayoutList[i].variantItemList[j].description == keyboard_value) {
                    keyboard_key = QString("%1|%2").arg(KeyboardLayoutList[i].name).arg(KeyboardLayoutList[i].variantItemList[j].name);
                    return keyboard_key;
                }
            }
        }
    }
    return keyboard_key;
}

bool XkbParser::parse() {

    QFile baseFile(kBaseFile);

    QDomDocument document;

    if (baseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        document.setContent(&baseFile);
    } else {
        qCWarning(DDE_SHELL) << "Failed to open base.xml";
        return false;
    }

    QDomElement rootElement = document.documentElement();

    if (rootElement.isNull()) {
        qCWarning(DDE_SHELL) << "Root element is null.";
        return false;
    }

    QDomElement layoutListElement = rootElement.firstChildElement("layoutList");

    QDomNodeList layoutNodes = layoutListElement.elementsByTagName("layout");

    if (layoutNodes.isEmpty()) {
        qCWarning(DDE_SHELL) << "Layout list is empty.";
        return false;
    }

    QDomNode layoutNode;
    for (int layoutIndex = 0; layoutIndex < layoutNodes.size(); ++layoutIndex) {
        VariantItem tmpVariantItem;

        LayoutItem tmpLayoutItem;

        layoutNode = layoutNodes.at(layoutIndex);

        QDomElement configElement = layoutNode.firstChildElement("configItem");
        QDomElement nameElement = configElement.firstChildElement("name");
        //get the Layout element: name
        tmpLayoutItem.name = nameElement.text();

        QDomElement descriptionElement = configElement.firstChildElement("description");
        //get the Layout element: description
        tmpLayoutItem.description = descriptionElement.text();

        QDomElement variantListElement = layoutNode.firstChildElement("variantList");
        QDomNodeList variantNodes = variantListElement.elementsByTagName("variant");
        QDomNode variantNode;
        for (int variantIndex = 0; variantIndex < variantNodes.size(); ++variantIndex) {
            variantNode = variantNodes.at(variantIndex);
            if (variantNode.isNull()) {
                qCWarning(DDE_SHELL) << "Variant node is null";
                continue;
            }

            QDomElement variantConfigElement = variantNode.firstChildElement("configItem");
            if (variantConfigElement.isNull()) {
                qCWarning(DDE_SHELL) << "Variant config element is null";
                continue;
            }
            QDomElement variantNameElement = variantConfigElement.firstChildElement("name");
            tmpVariantItem.name = variantNameElement.text();
            QDomElement variantDescriptionElement = variantConfigElement.firstChildElement("description");
            tmpVariantItem.description = variantDescriptionElement.text();

            tmpLayoutItem.variantItemList.append(tmpVariantItem);
        }
        KeyboardLayoutList.append(tmpLayoutItem);
    }
    return true;
}

XkbParser::~XkbParser() {

}
