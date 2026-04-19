/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "FindAndReadFileTool.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <logger/Logger.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

FindAndReadFileTool::FindAndReadFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString FindAndReadFileTool::id() const
{
    return "find_and_read_file";
}

QString FindAndReadFileTool::displayName() const
{
    return "Finding and reading file";
}

QString FindAndReadFileTool::description() const
{
    return "Search for a file by name/path and optionally read its content. "
           "Returns the best matching file and its content.";
}

QJsonObject FindAndReadFileTool::parametersSchema() const
{
    QJsonObject properties;

    properties["query"] = QJsonObject{
        {"type", "string"},
        {"description", "Filename, partial name, or path to search for (case-insensitive)"}};

    properties["file_pattern"] = QJsonObject{
        {"type", "string"}, {"description", "File pattern filter (e.g., '*.cpp', '*.h', '*.qml')"}};

    properties["read_content"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Read file content in addition to finding path (default: true)"}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"query"};

    return definition;
}

QFuture<LLMQore::ToolResult> FindAndReadFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> LLMQore::ToolResult {
        QString query = input["query"].toString().trimmed();
        if (query.isEmpty()) {
            throw LLMQore::ToolInvalidArgument("Query parameter is required");
        }

        QString filePattern = input["file_pattern"].toString();
        bool readContent = input["read_content"].toBool(true);

        LOG_MESSAGE(QString("FindAndReadFileTool: Searching for '%1' (pattern: %2, read: %3)")
                        .arg(query, filePattern.isEmpty() ? "none" : filePattern)
                        .arg(readContent));

        FileSearchUtils::FileMatch bestMatch = FileSearchUtils::findBestMatch(
            query, filePattern, 10, m_ignoreManager);

        if (bestMatch.absolutePath.isEmpty()) {
            return LLMQore::ToolResult::text(QString("No file found matching '%1'").arg(query));
        }

        if (readContent) {
            bestMatch.content = FileSearchUtils::readFileContent(bestMatch.absolutePath);
            if (bestMatch.content.isNull()) {
                bestMatch.error = "Could not read file";
            }
        }

        return LLMQore::ToolResult::text(formatResult(bestMatch, readContent));
    });
}

QString FindAndReadFileTool::formatResult(const FileSearchUtils::FileMatch &match,
                                          bool readContent) const
{
    QString result
        = QString("Found file: %1\nAbsolute path: %2").arg(match.relativePath, match.absolutePath);

    if (readContent) {
        if (!match.error.isEmpty()) {
            result += QString("\nError: %1").arg(match.error);
        } else {
            result += QString("\n\n=== Content ===\n%1").arg(match.content);
        }
    }

    return result;
}

} // namespace QodeAssist::Tools
