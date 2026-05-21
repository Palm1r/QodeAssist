// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace QodeAssist::TomlSerializer {

[[nodiscard]] QString escapeBasic(const QString &s);

class TomlWriter
{
public:
    TomlWriter() = default;
    explicit TomlWriter(int keyColumnWidth) : m_keyColumnWidth(keyColumnWidth) {}

    void setKeyColumnWidth(int width) { m_keyColumnWidth = width; }

    void writeBlankLine();
    void writeComment(const QString &line);     // "# line\n"
    void writeTableHeader(const QString &name); // "[name]\n"

    void writeString(const QString &key, const QString &value);
    void writeBool(const QString &key, bool value);
    void writeInt(const QString &key, qint64 value);
    void writeDouble(const QString &key, double value);
    void writeStringArray(const QString &key, const QStringList &values);

    void writeJsonPrimitives(const QJsonObject &obj);

    void writeStringDict(const QHash<QString, QString> &dict);

    [[nodiscard]] QString result() const { return m_out; }
    [[nodiscard]] QByteArray toUtf8() const { return m_out.toUtf8(); }

private:
    void writeKeyPrefix(const QString &key);

    QString m_out;
    int m_keyColumnWidth = 0;
};

} // namespace QodeAssist::TomlSerializer
