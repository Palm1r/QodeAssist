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

#include "SearchInProjectTool.hpp"

#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

SearchInProjectTool::SearchInProjectTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString SearchInProjectTool::name() const
{
    return "search_in_project";
}

QString SearchInProjectTool::stringName() const
{
    return {"Searching in project files"};
}

QString SearchInProjectTool::description() const
{
    return "Search for text or patterns across all project files. "
           "Returns matching lines with file paths, line numbers, and context. "
           "Supports plain text, regex, case-sensitive/insensitive search, whole word matching, "
           "and file pattern filtering.";
}

QJsonObject SearchInProjectTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject queryProperty;
    queryProperty["type"] = "string";
    queryProperty["description"] = "Text or regex pattern to search for";
    properties["query"] = queryProperty;

    QJsonObject caseSensitiveProperty;
    caseSensitiveProperty["type"] = "boolean";
    caseSensitiveProperty["description"] = "Enable case-sensitive search";
    properties["case_sensitive"] = caseSensitiveProperty;

    QJsonObject useRegexProperty;
    useRegexProperty["type"] = "boolean";
    useRegexProperty["description"] = "Treat query as regular expression";
    properties["use_regex"] = useRegexProperty;

    QJsonObject wholeWordsProperty;
    wholeWordsProperty["type"] = "boolean";
    wholeWordsProperty["description"] = "Match whole words only";
    properties["whole_words"] = wholeWordsProperty;

    QJsonObject filePatternProperty;
    filePatternProperty["type"] = "string";
    filePatternProperty["description"] = "File pattern to filter results (e.g., '*.cpp', '*.h')";
    properties["file_pattern"] = filePatternProperty;

    QJsonObject maxResultsProperty;
    maxResultsProperty["type"] = "integer";
    maxResultsProperty["description"] = "Maximum number of results to return (default: 50)";
    properties["max_results"] = maxResultsProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("query");
    definition["required"] = required;

    switch (format) {
    case LLMCore::ToolSchemaFormat::OpenAI:
        return customizeForOpenAI(definition);
    case LLMCore::ToolSchemaFormat::Claude:
        return customizeForClaude(definition);
    case LLMCore::ToolSchemaFormat::Ollama:
        return customizeForOllama(definition);
    case LLMCore::ToolSchemaFormat::Google:
        return customizeForGoogle(definition);
    }
    return definition;
}

LLMCore::ToolPermissions SearchInProjectTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> SearchInProjectTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString query = input["query"].toString();
        if (query.isEmpty()) {
            QString error = "Error: query parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        bool caseSensitive = input["case_sensitive"].toBool(false);
        bool useRegex = input["use_regex"].toBool(false);
        bool wholeWords = input["whole_words"].toBool(false);
        QString filePattern = input["file_pattern"].toString();
        int maxResults = input["max_results"].toInt(50);

        LOG_MESSAGE(QString("Searching for: '%1' (case_sensitive: %2, regex: %3, whole_words: %4)")
                        .arg(query)
                        .arg(caseSensitive)
                        .arg(useRegex)
                        .arg(wholeWords));

        QList<SearchResult> results
            = searchInFiles(query, caseSensitive, useRegex, wholeWords, filePattern);

        if (results.isEmpty()) {
            return QString("No matches found for '%1'").arg(query);
        }

        return formatResults(results, maxResults);
    });
}

QList<SearchInProjectTool::SearchResult> SearchInProjectTool::searchInFiles(
    const QString &searchText,
    bool caseSensitive,
    bool useRegex,
    bool wholeWords,
    const QString &filePattern) const
{
    QList<SearchResult> results;

    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();
    if (projects.isEmpty()) {
        LOG_MESSAGE("No projects found");
        return results;
    }

    QRegularExpression searchRegex;
    if (useRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::MultilineOption;
        if (!caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        searchRegex.setPattern(searchText);
        searchRegex.setPatternOptions(options);

        if (!searchRegex.isValid()) {
            LOG_MESSAGE(QString("Invalid regex pattern: %1").arg(searchText));
            return results;
        }
    }

    QRegularExpression filePatternRegex;
    if (!filePattern.isEmpty()) {
        QString pattern = QRegularExpression::wildcardToRegularExpression(filePattern);
        filePatternRegex.setPattern(pattern);
    }

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);

        for (const auto &filePath : projectFiles) {
            QString absolutePath = filePath.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            if (!filePattern.isEmpty()) {
                QFileInfo fileInfo(absolutePath);
                if (!filePatternRegex.match(fileInfo.fileName()).hasMatch()) {
                    continue;
                }
            }

            QFile file(absolutePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }

            QTextStream stream(&file);
            stream.setAutoDetectUnicode(true);
            int lineNumber = 0;

            while (!stream.atEnd()) {
                lineNumber++;
                QString line = stream.readLine();

                bool matched = false;

                if (useRegex) {
                    matched = searchRegex.match(line).hasMatch();
                } else {
                    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive
                                                           : Qt::CaseInsensitive;

                    if (wholeWords) {
                        QRegularExpression wordRegex(
                            QString("\\b%1\\b").arg(QRegularExpression::escape(searchText)),
                            caseSensitive ? QRegularExpression::NoPatternOption
                                          : QRegularExpression::CaseInsensitiveOption);
                        matched = wordRegex.match(line).hasMatch();
                    } else {
                        matched = line.contains(searchText, cs);
                    }
                }

                if (matched) {
                    SearchResult result;
                    result.filePath = absolutePath;
                    result.lineNumber = lineNumber;
                    result.lineContent = line.trimmed();

                    QString context;
                    long long currentPos = stream.pos();
                    stream.seek(0);
                    int contextLineNum = 0;
                    while (contextLineNum < lineNumber - 1 && !stream.atEnd()) {
                        stream.readLine();
                        contextLineNum++;
                    }

                    QStringList contextLines;
                    for (int i = qMax(1, lineNumber - 2); i < lineNumber; ++i) {
                        if (!stream.atEnd()) {
                            contextLines.append(stream.readLine().trimmed());
                        }
                    }

                    if (!contextLines.isEmpty()) {
                        result.context = contextLines.join("\n");
                    }

                    stream.seek(currentPos);

                    results.append(result);
                }
            }

            file.close();
        }
    }

    return results;
}

QString SearchInProjectTool::formatResults(const QList<SearchResult> &results, int maxResults) const
{
    QString output = QString("Found %1 matches:\n\n").arg(results.size());

    int displayCount = qMin(results.size(), maxResults);
    for (int i = 0; i < displayCount; ++i) {
        const SearchResult &result = results[i];

        output += QString("%1:%2\n").arg(result.filePath).arg(result.lineNumber);
        output += QString("  %1\n").arg(result.lineContent);

        if (!result.context.isEmpty()) {
            output += QString("  Context:\n");
            for (const QString &contextLine : result.context.split('\n')) {
                output += QString("    %1\n").arg(contextLine);
            }
        }

        output += "\n";
    }

    if (results.size() > maxResults) {
        output += QString("... and %1 more matches\n").arg(results.size() - maxResults);
    }

    return output.trimmed();
}

} // namespace QodeAssist::Tools
