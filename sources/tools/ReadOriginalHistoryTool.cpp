// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ReadOriginalHistoryTool.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>

#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"

namespace QodeAssist::Tools {

namespace {

QString roleName(Session::RowKind kind)
{
    switch (kind) {
    case Session::RowKind::System:
        return QStringLiteral("system");
    case Session::RowKind::User:
        return QStringLiteral("user");
    case Session::RowKind::Assistant:
        return QStringLiteral("assistant");
    case Session::RowKind::Tool:
        return QStringLiteral("tool");
    case Session::RowKind::FileEdit:
        return QStringLiteral("file_edit");
    case Session::RowKind::Thinking:
        return QStringLiteral("thinking");
    }
    return QStringLiteral("unknown");
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString resolveRootHistoryPath(const QString &sessionPath)
{
    QString current = sessionPath;
    QString rootPath;

    for (int depth = 0; depth < 32; ++depth) {
        const QJsonObject obj = readJsonObject(current);
        const QString parent = obj.value("compressedFrom").toString();
        if (parent.isEmpty() || parent == current)
            break;
        if (!QFile::exists(parent))
            break;
        rootPath = parent;
        current = parent;
    }

    return rootPath;
}

} // namespace

ReadOriginalHistoryTool::ReadOriginalHistoryTool(QObject *parent)
    : BaseTool(parent)
{}

QString ReadOriginalHistoryTool::id() const
{
    return "read_original_history";
}

QString ReadOriginalHistoryTool::displayName() const
{
    return "Reading pre-compression history";
}

QString ReadOriginalHistoryTool::description() const
{
    return "Read the original, full chat history from before this conversation was "
           "compressed into a summary. Use this only when the summary in context is "
           "missing a detail you need (an exact code snippet, file path, decision, or "
           "wording). The result can be large, so prefer the 'query' parameter to search "
           "and 'offset'/'limit' to page through messages. Returns nothing useful if the "
           "conversation was never compressed.";
}

QJsonObject ReadOriginalHistoryTool::parametersSchema() const
{
    QJsonObject properties;

    properties["query"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Optional case-insensitive substring. When set, only messages whose content "
         "contains it are returned."}};

    properties["role"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Optional role filter: 'user', 'assistant', 'system' or 'tool'."}};

    properties["offset"] = QJsonObject{
        {"type", "integer"},
        {"description", "Index of the first matching message to return (default 0)."}};

    properties["limit"] = QJsonObject{
        {"type", "integer"},
        {"description", "Maximum number of messages to return (default 20)."}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{};

    return definition;
}

QFuture<LLMQore::ToolResult> ReadOriginalHistoryTool::executeAsync(const QJsonObject &input)
{
    QString sessionPath;
    {
        QMutexLocker locker(&m_mutex);
        sessionPath = m_currentSessionId;
    }

    return QtConcurrent::run([input, sessionPath]() -> LLMQore::ToolResult {
        if (sessionPath.isEmpty()) {
            throw LLMQore::ToolRuntimeError(
                "No active chat session, cannot locate pre-compression history.");
        }

        const QString rootPath = resolveRootHistoryPath(sessionPath);
        if (rootPath.isEmpty()) {
            return LLMQore::ToolResult::text(
                "This conversation was never compressed; there is no separate "
                "pre-compression history. The messages already in context are the full "
                "history.");
        }

        const auto history = Session::HistorySerializer::fromJson(readJsonObject(rootPath));
        if (!history) {
            return LLMQore::ToolResult::text(
                QString("Cannot read the pre-compression history at %1: unsupported chat file "
                        "format.")
                    .arg(rootPath));
        }

        const QList<Session::MessageRow> messages = Session::projectToRows(*history);

        const QString query = input.value("query").toString().trimmed();
        const QString roleFilter = input.value("role").toString().trimmed().toLower();
        const int offset = qMax(0, input.value("offset").toInt(0));
        const int limit = qBound(1, input.value("limit").toInt(20), 200);

        QStringList matched;
        int matchCount = 0;
        for (int i = 0; i < messages.size(); ++i) {
            const Session::MessageRow &msg = messages.at(i);
            const QString role = roleName(msg.kind);
            const QString content = msg.content;

            if (!roleFilter.isEmpty() && role != roleFilter)
                continue;
            if (!query.isEmpty() && !content.contains(query, Qt::CaseInsensitive))
                continue;

            ++matchCount;
            if (matchCount <= offset || matched.size() >= limit)
                continue;

            matched.append(QString("[#%1 %2]\n%3").arg(i).arg(role, content));
        }

        const int shown = matched.size();
        QString header = QString("Pre-compression history (%1): %2 matching message(s)")
                             .arg(rootPath)
                             .arg(matchCount);
        if (shown < matchCount || offset > 0) {
            header += QString(", showing %1-%2")
                          .arg(offset + 1)
                          .arg(offset + shown);
        }

        if (shown == 0)
            return LLMQore::ToolResult::text(header + "\n\nNo messages to display.");

        return LLMQore::ToolResult::text(header + "\n\n" + matched.join("\n\n---\n\n"));
    });
}

void ReadOriginalHistoryTool::setCurrentSessionId(const QString &sessionId)
{
    QMutexLocker locker(&m_mutex);
    m_currentSessionId = sessionId;
}

} // namespace QodeAssist::Tools
