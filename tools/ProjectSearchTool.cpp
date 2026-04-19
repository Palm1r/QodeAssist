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

#include "ProjectSearchTool.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <cplusplus/Overview.h>
#include <cplusplus/Scope.h>
#include <cplusplus/Symbols.h>
#include <cppeditor/cppmodelmanager.h>
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

ProjectSearchTool::ProjectSearchTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString ProjectSearchTool::id() const
{
    return "search_project";
}

QString ProjectSearchTool::displayName() const
{
    return "Searching in project";
}

QString ProjectSearchTool::description() const
{
    return "Search across all open Qt Creator project(s) for text occurrences or C++ symbol "
           "definitions. 'text' mode scans source files line-by-line for literal text or regex. "
           "'symbol' mode uses Qt Creator's C++ code model and works only for C++ "
           "(not QML, Python, or plain text). Respects .qodeassistignore. "
           "Use `find_file` for locating files by name, not this tool.";
}

QJsonObject ProjectSearchTool::parametersSchema() const
{
    QJsonObject properties;

    properties["query"] = QJsonObject{
        {"type", "string"},
        {"description", "Text string or symbol name to search for. In text mode with "
                        "use_regex=true, this is a regular expression."}};

    properties["search_type"] = QJsonObject{
        {"type", "string"},
        {"enum", QJsonArray{"text", "symbol"}},
        {"description", "Search mode: 'text' scans file contents, 'symbol' looks up C++ "
                        "declarations via the code model."}};

    properties["symbol_type"] = QJsonObject{
        {"type", "string"},
        {"enum", QJsonArray{"all", "class", "function", "enum", "variable", "namespace"}},
        {"description", "Filter for symbol mode. Default: 'all'. Ignored in text mode."}};

    properties["case_sensitive"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Case-sensitive matching. Default: false."}};

    properties["use_regex"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Treat the query as a regular expression. Default: false."}};

    properties["whole_words"] = QJsonObject{
        {"type", "boolean"},
        {"description", "Match whole words only. Text mode only; ignored in symbol mode. "
                        "Default: false."}};

    properties["file_pattern"] = QJsonObject{
        {"type", "string"},
        {"description", "Wildcard to restrict which files are searched in text mode, e.g. "
                        "'*.cpp' or '*.h'. Default: all files."}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"query", "search_type"};

    return definition;
}

QFuture<LLMQore::ToolResult> ProjectSearchTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> LLMQore::ToolResult {
        QString query = input["query"].toString().trimmed();
        if (query.isEmpty()) {
            throw LLMQore::ToolInvalidArgument("Query parameter is required");
        }

        QString searchTypeStr = input["search_type"].toString();
        if (searchTypeStr != "text" && searchTypeStr != "symbol") {
            throw LLMQore::ToolInvalidArgument("search_type must be 'text' or 'symbol'");
        }

        SearchType searchType = (searchTypeStr == "symbol") ? SearchType::Symbol : SearchType::Text;
        QList<SearchResult> results;

        if (searchType == SearchType::Text) {
            bool caseSensitive = input["case_sensitive"].toBool(false);
            bool useRegex = input["use_regex"].toBool(false);
            bool wholeWords = input["whole_words"].toBool(false);
            QString filePattern = input["file_pattern"].toString();

            results = searchText(query, caseSensitive, useRegex, wholeWords, filePattern);
        } else {
            SymbolType symbolType = parseSymbolType(input["symbol_type"].toString());
            bool caseSensitive = input["case_sensitive"].toBool(false);
            bool useRegex = input["use_regex"].toBool(false);

            results = searchSymbols(query, symbolType, caseSensitive, useRegex);
        }

        if (results.isEmpty()) {
            return LLMQore::ToolResult::text(QString("No matches found for '%1'").arg(query));
        }

        return LLMQore::ToolResult::text(formatResults(results, query));
    });
}

QList<ProjectSearchTool::SearchResult> ProjectSearchTool::searchText(
    const QString &query,
    bool caseSensitive,
    bool useRegex,
    bool wholeWords,
    const QString &filePattern)
{
    QList<SearchResult> results;
    auto projects = ProjectExplorer::ProjectManager::projects();
    if (projects.isEmpty())
        return results;

    QRegularExpression searchRegex;
    if (useRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::MultilineOption;
        if (!caseSensitive)
            options |= QRegularExpression::CaseInsensitiveOption;
        searchRegex.setPattern(query);
        searchRegex.setPatternOptions(options);
        if (!searchRegex.isValid())
            return results;
    }

    QRegularExpression fileFilter;
    if (!filePattern.isEmpty()) {
        fileFilter.setPattern(QRegularExpression::wildcardToRegularExpression(filePattern));
    }

    for (auto project : projects) {
        if (!project)
            continue;

        auto projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        QString projectDir = project->projectDirectory().path();

        for (const auto &filePath : projectFiles) {
            QString absolutePath = filePath.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project))
                continue;

            if (!filePattern.isEmpty()) {
                QFileInfo fileInfo(absolutePath);
                if (!fileFilter.match(fileInfo.fileName()).hasMatch())
                    continue;
            }

            QFile file(absolutePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream stream(&file);
            int lineNumber = 0;
            while (!stream.atEnd()) {
                lineNumber++;
                QString line = stream.readLine();
                bool matched = false;

                if (useRegex) {
                    matched = searchRegex.match(line).hasMatch();
                } else {
                    auto cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                    if (wholeWords) {
                        QRegularExpression wordRegex(
                            QString("\\b%1\\b").arg(QRegularExpression::escape(query)),
                            caseSensitive ? QRegularExpression::NoPatternOption
                                          : QRegularExpression::CaseInsensitiveOption);
                        matched = wordRegex.match(line).hasMatch();
                    } else {
                        matched = line.contains(query, cs);
                    }
                }

                if (matched) {
                    SearchResult result;
                    result.filePath = absolutePath;
                    result.relativePath = QDir(projectDir).relativeFilePath(absolutePath);
                    result.content = line.trimmed();
                    result.lineNumber = lineNumber;
                    results.append(result);
                }
            }
        }
    }
    return results;
}

QList<ProjectSearchTool::SearchResult> ProjectSearchTool::searchSymbols(
    const QString &query, SymbolType symbolType, bool caseSensitive, bool useRegex)
{
    QList<SearchResult> results;
    auto modelManager = CppEditor::CppModelManager::instance();
    if (!modelManager)
        return results;

    QRegularExpression searchRegex;
    if (useRegex) {
        QRegularExpression::PatternOptions options = caseSensitive
                                                         ? QRegularExpression::NoPatternOption
                                                         : QRegularExpression::CaseInsensitiveOption;
        searchRegex.setPattern(query);
        searchRegex.setPatternOptions(options);
        if (!searchRegex.isValid())
            return results;
    }

    CPlusPlus::Overview overview;
    auto snapshot = modelManager->snapshot();

    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        auto document = it.value();
        if (!document || !document->globalNamespace())
            continue;

        QString filePath = document->filePath().path();
        if (m_ignoreManager->shouldIgnore(filePath, nullptr))
            continue;

        auto searchInScope = [&](auto self, CPlusPlus::Scope *scope) -> void {
            if (!scope)
                return;

            for (unsigned i = 0; i < scope->memberCount(); ++i) {
                auto symbol = scope->memberAt(i);
                if (!symbol || !symbol->name())
                    continue;

                QString symbolName = overview.prettyName(symbol->name());
                bool nameMatches = false;

                if (useRegex) {
                    nameMatches = searchRegex.match(symbolName).hasMatch();
                } else {
                    nameMatches = caseSensitive
                                      ? symbolName == query
                                      : symbolName.compare(query, Qt::CaseInsensitive) == 0;
                }

                bool typeMatches = (symbolType == SymbolType::All)
                                   || (symbolType == SymbolType::Class && symbol->asClass())
                                   || (symbolType == SymbolType::Function && symbol->asFunction())
                                   || (symbolType == SymbolType::Enum && symbol->asEnum())
                                   || (symbolType == SymbolType::Variable && symbol->asDeclaration())
                                   || (symbolType == SymbolType::Namespace && symbol->asNamespace());

                if (nameMatches && typeMatches) {
                    SearchResult result;
                    result.filePath = filePath;

                    auto projects = ProjectExplorer::ProjectManager::projects();
                    if (!projects.isEmpty()) {
                        QString projectDir = projects.first()->projectDirectory().path();
                        result.relativePath = QDir(projectDir).relativeFilePath(filePath);
                    } else {
                        result.relativePath = QFileInfo(filePath).fileName();
                    }

                    result.content = symbolName;
                    result.lineNumber = symbol->line();
                    result.context = overview.prettyType(symbol->type());
                    results.append(result);
                }

                if (auto nestedScope = symbol->asScope()) {
                    self(self, nestedScope);
                }
            }
        };

        searchInScope(searchInScope, document->globalNamespace());
    }

    return results;
}

ProjectSearchTool::SymbolType ProjectSearchTool::parseSymbolType(const QString &typeStr)
{
    if (typeStr == "class")
        return SymbolType::Class;
    if (typeStr == "function")
        return SymbolType::Function;
    if (typeStr == "enum")
        return SymbolType::Enum;
    if (typeStr == "variable")
        return SymbolType::Variable;
    if (typeStr == "namespace")
        return SymbolType::Namespace;
    return SymbolType::All;
}

QString ProjectSearchTool::formatResults(const QList<SearchResult> &results, const QString &query)
{
    QString output = QString("Query: %1\n Found %2 matches:\n\n").arg(query).arg(results.size());
    int count = 0;
    for (const auto &r : results) {
        if (++count > 100) {
            output += QString("... and %1 more matches").arg(results.size() - 20);
            break;
        }
        output += QString("%1:%2: %3\n").arg(r.relativePath).arg(r.lineNumber).arg(r.content);
    }
    return output;
}

} // namespace QodeAssist::Tools
