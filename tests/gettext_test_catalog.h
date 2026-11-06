// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDataStream>
#include <QFile>
#include <QMap>

inline bool writeGettextCatalog(const QString &path, const QMap<QByteArray, QByteArray> &messages,
                               QDataStream::ByteOrder order = QDataStream::LittleEndian)
{
    QByteArray catalog;
    QDataStream stream(&catalog, QIODevice::WriteOnly);
    stream.setByteOrder(order);
    const quint32 count = messages.size();
    stream << quint32(0x950412de) << quint32(0) << count << quint32(28)
           << quint32(28 + count * 8) << quint32(0) << quint32(0);
    QByteArray strings;
    for (const auto &entries : {messages.keys(), messages.values()}) {
        for (const auto &entry : entries) {
            stream << quint32(entry.size()) << quint32(28 + count * 16 + strings.size());
            strings += entry + '\0';
        }
    }
    stream.writeRawData(strings.constData(), strings.size());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(catalog) == catalog.size();
}
