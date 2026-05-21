// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TomlWriter.hpp"

#include <QJsonValue>

namespace QodeAssist::TomlSerializer {

QString escapeBasic(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        const ushort u = c.unicode();
        switch (u) {
        case '\\': out += QLatin1String("\\\\"); break;
        case '"':  out += QLatin1String("\\\""); break;
        case '\b': out += QLatin1String("\\b"); break;
        case '\t': out += QLatin1String("\\t"); break;
        case '\n': out += QLatin1String("\\n"); break;
        case '\f': out += QLatin1String("\\f"); break;
        case '\r': out += QLatin1String("\\r"); break;
        default:
            if (u < 0x20 || u == 0x7f)
                out += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
            else
                out += c;
            break;
        }
    }
    return out;
}

void TomlWriter::writeBlankLine()
{
    m_out += QLatin1Char('\n');
}

void TomlWriter::writeComment(const QString &line)
{
    m_out += QLatin1String("# ");
    m_out += line;
    m_out += QLatin1Char('\n');
}

void TomlWriter::writeTableHeader(const QString &name)
{
    m_out += QLatin1Char('[');
    m_out += name;
    m_out += QLatin1String("]\n");
}

void TomlWriter::writeKeyPrefix(const QString &key)
{
    m_out += key;
    if (m_keyColumnWidth > key.size())
        m_out += QString(m_keyColumnWidth - key.size(), QLatin1Char(' '));
    m_out += QLatin1String(" = ");
}

void TomlWriter::writeString(const QString &key, const QString &value)
{
    writeKeyPrefix(key);
    m_out += QLatin1Char('"');
    m_out += escapeBasic(value);
    m_out += QLatin1String("\"\n");
}

void TomlWriter::writeBool(const QString &key, bool value)
{
    writeKeyPrefix(key);
    m_out += value ? QLatin1String("true") : QLatin1String("false");
    m_out += QLatin1Char('\n');
}

void TomlWriter::writeInt(const QString &key, qint64 value)
{
    writeKeyPrefix(key);
    m_out += QString::number(value);
    m_out += QLatin1Char('\n');
}

void TomlWriter::writeDouble(const QString &key, double value)
{
    writeKeyPrefix(key);
    m_out += QString::number(value);
    m_out += QLatin1Char('\n');
}

void TomlWriter::writeStringArray(const QString &key, const QStringList &values)
{
    writeKeyPrefix(key);
    m_out += QLatin1Char('[');
    bool first = true;
    for (const QString &v : values) {
        if (!first)
            m_out += QLatin1String(", ");
        m_out += QLatin1Char('"');
        m_out += escapeBasic(v);
        m_out += QLatin1Char('"');
        first = false;
    }
    m_out += QLatin1String("]\n");
}

void TomlWriter::writeJsonPrimitives(const QJsonObject &obj)
{
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QJsonValue &v = it.value();
        switch (v.type()) {
        case QJsonValue::String: writeString(it.key(), v.toString()); break;
        case QJsonValue::Bool:   writeBool(it.key(), v.toBool()); break;
        case QJsonValue::Double: {
            const double d = v.toDouble();
            const qint64 i = static_cast<qint64>(d);
            if (static_cast<double>(i) == d)
                writeInt(it.key(), i);
            else
                writeDouble(it.key(), d);
            break;
        }
        default:
            break;
        }
    }
}

void TomlWriter::writeStringDict(const QHash<QString, QString> &dict)
{
    for (auto it = dict.constBegin(); it != dict.constEnd(); ++it)
        writeString(it.key(), it.value());
}

} // namespace QodeAssist::TomlSerializer
