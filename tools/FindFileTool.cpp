// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FindFileTool.hpp"

#include "FileSearchUtils.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <logger/Logger.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

FindFileTool::FindFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString FindFileTool::id() const
{
    return "find_file";
}

QString FindFileTool::displayName() const
{
    return "Finding file";
}

QString FindFileTool::description() const
{
    return "Find a file in the current project(s) by name or partial path. "
           "Returns the absolute path, project-relative path, and the name of the project "
           "that contains the match. Does NOT read file content — use `read_file` separately "
           "when you need the content. Use this when you know part of the filename but not "
           "the exact location.";
}

QJsonObject FindFileTool::parametersSchema() const
{
    QJsonObject properties;

    properties["query"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Filename, partial filename, or partial path to look for. Case-insensitive. "
         "Exact filename matches rank highest, then path matches, then partial name matches."}};

    properties["file_pattern"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Optional wildcard filter applied to the filename. Examples: '*.cpp', '*.h', '*.qml'. "
         "Leave empty to match any extension."}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"query"};

    return definition;
}

QFuture<LLMQore::ToolResult> FindFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> LLMQore::ToolResult {
        QString query = input["query"].toString().trimmed();
        if (query.isEmpty()) {
            throw LLMQore::ToolInvalidArgument("'query' parameter is required and cannot be empty");
        }

        QString filePattern = input["file_pattern"].toString();

        LOG_MESSAGE(QString("FindFileTool: Searching for '%1' (pattern: %2)")
                        .arg(query, filePattern.isEmpty() ? "none" : filePattern));

        FileSearchUtils::FileMatch match
            = FileSearchUtils::findBestMatch(query, filePattern, 10, m_ignoreManager);

        if (match.absolutePath.isEmpty()) {
            return LLMQore::ToolResult::text(QString("No file found matching '%1'").arg(query));
        }

        QString result = QString("Found file: %1\nAbsolute path: %2\nProject: %3")
                             .arg(match.relativePath, match.absolutePath, match.projectName);

        return LLMQore::ToolResult::text(result);
    });
}

} // namespace QodeAssist::Tools
