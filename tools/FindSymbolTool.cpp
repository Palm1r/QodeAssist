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

#include "FindSymbolTool.hpp"
#include "ToolExceptions.hpp"

#include <cplusplus/Overview.h>
#include <cplusplus/Scope.h>
#include <cplusplus/Symbols.h>
#include <cppeditor/cppmodelmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <texteditor/textdocument.h>
#include <utils/filepath.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

FindSymbolTool::FindSymbolTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString FindSymbolTool::name() const
{
    return "find_cpp_symbol";
}

QString FindSymbolTool::stringName() const
{
    return "Finding C++ symbols in project";
}

QString FindSymbolTool::description() const
{
    return "Find C++ symbols (classes, functions, enums, variables, typedefs, namespaces) in the project. "
           "Returns file paths, line numbers, qualified names, and optionally source code. "
           "Supports exact match, wildcard patterns, and regular expressions.";
}

QJsonObject FindSymbolTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject symbolNameProperty;
    symbolNameProperty["type"] = "string";
    symbolNameProperty["description"] = "Name or pattern of the symbol to find (supports exact "
                                        "match, wildcard, or regex depending on flags)";
    properties["symbol_name"] = symbolNameProperty;

    QJsonObject symbolTypeProperty;
    symbolTypeProperty["type"] = "string";
    symbolTypeProperty["description"]
        = "Type of symbol: all, class, function, enum, variable, typedef, namespace";
    symbolTypeProperty["enum"]
        = QJsonArray{"all", "class", "function", "enum", "variable", "typedef", "namespace"};
    properties["symbol_type"] = symbolTypeProperty;

    QJsonObject scopeFilterProperty;
    scopeFilterProperty["type"] = "string";
    scopeFilterProperty["description"]
        = "Filter results by scope (e.g., 'MyNamespace', 'MyClass')";
    properties["scope_filter"] = scopeFilterProperty;

    QJsonObject caseSensitiveProperty;
    caseSensitiveProperty["type"] = "boolean";
    caseSensitiveProperty["description"] = "Enable case-sensitive search (default: true)";
    properties["case_sensitive"] = caseSensitiveProperty;

    QJsonObject useRegexProperty;
    useRegexProperty["type"] = "boolean";
    useRegexProperty["description"]
        = "Treat symbol_name as regular expression (default: false)";
    properties["use_regex"] = useRegexProperty;

    QJsonObject useWildcardProperty;
    useWildcardProperty["type"] = "boolean";
    useWildcardProperty["description"]
        = "Treat symbol_name as wildcard pattern like 'find*', '*Symbol' (default: false)";
    properties["use_wildcard"] = useWildcardProperty;

    QJsonObject includeCodeProperty;
    includeCodeProperty["type"] = "boolean";
    includeCodeProperty["description"] = "Include source code of found symbols";
    properties["include_code"] = includeCodeProperty;

    QJsonObject maxResultsProperty;
    maxResultsProperty["type"] = "integer";
    maxResultsProperty["description"] = "Maximum number of results to return";
    properties["max_results"] = maxResultsProperty;

    QJsonObject groupByProperty;
    groupByProperty["type"] = "string";
    groupByProperty["description"] = "How to group results: type, file, or scope";
    groupByProperty["enum"] = QJsonArray{"type", "file", "scope"};
    properties["group_by"] = groupByProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"symbol_name"};

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

LLMCore::ToolPermissions FindSymbolTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> FindSymbolTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString symbolName = input["symbol_name"].toString();
        QString symbolTypeStr = input["symbol_type"].toString("all");
        QString scopeFilter = input["scope_filter"].toString();
        bool caseSensitive = input["case_sensitive"].toBool(true);
        bool useRegex = input["use_regex"].toBool(false);
        bool useWildcard = input["use_wildcard"].toBool(false);
        bool includeCode = input["include_code"].toBool(false);
        int maxResults = input["max_results"].toInt(10);
        QString groupBy = input["group_by"].toString("type");

        if (symbolName.isEmpty()) {
            QString error = "Error: 'symbol_name' parameter is required";
            LOG_MESSAGE(error);
            throw ToolInvalidArgument(error);
        }

        if (useRegex && useWildcard) {
            QString error = "Error: 'use_regex' and 'use_wildcard' cannot be used together";
            LOG_MESSAGE(error);
            throw ToolInvalidArgument(error);
        }

        SymbolType type = parseSymbolType(symbolTypeStr);
        LOG_MESSAGE(QString("Searching for symbol: '%1', type: %2, scope: '%3', "
                            "case_sensitive: %4, regex: %5, wildcard: %6")
                        .arg(symbolName, symbolTypeStr, scopeFilter)
                        .arg(caseSensitive)
                        .arg(useRegex)
                        .arg(useWildcard));

        QList<SymbolInfo> symbols
            = findSymbols(symbolName, type, scopeFilter, caseSensitive, useRegex, useWildcard);

        if (symbols.isEmpty()) {
            QString msg = QString("No symbol matching '%1' found in the project").arg(symbolName);
            if (!scopeFilter.isEmpty()) {
                msg += QString(" within scope '%1'").arg(scopeFilter);
            }
            return msg;
        }

        if (symbols.size() > maxResults) {
            symbols = symbols.mid(0, maxResults);
        }

        if (includeCode) {
            for (SymbolInfo &info : symbols) {
                info.code = extractSymbolCode(info);
            }
        }

        return formatResults(symbols, includeCode, groupBy);
    });
}

FindSymbolTool::SymbolType FindSymbolTool::parseSymbolType(const QString &typeStr) const
{
    if (typeStr == "class")
        return SymbolType::Class;
    if (typeStr == "function")
        return SymbolType::Function;
    if (typeStr == "enum")
        return SymbolType::Enum;
    if (typeStr == "variable")
        return SymbolType::Variable;
    if (typeStr == "typedef")
        return SymbolType::Typedef;
    if (typeStr == "namespace")
        return SymbolType::Namespace;
    return SymbolType::All;
}

QList<FindSymbolTool::SymbolInfo> FindSymbolTool::findSymbols(
    const QString &symbolName,
    SymbolType type,
    const QString &scopeFilter,
    bool caseSensitive,
    bool useRegex,
    bool useWildcard) const
{
    QList<SymbolInfo> results;

    auto *modelManager = CppEditor::CppModelManager::instance();
    if (!modelManager) {
        LOG_MESSAGE("CppModelManager not available");
        return results;
    }

    QRegularExpression searchPattern;
    if (useRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        searchPattern.setPattern(symbolName);
        searchPattern.setPatternOptions(options);

        if (!searchPattern.isValid()) {
            LOG_MESSAGE(QString("Invalid regex pattern: %1").arg(symbolName));
            return results;
        }
    } else if (useWildcard) {
        QString regexPattern = QRegularExpression::wildcardToRegularExpression(symbolName);
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        searchPattern.setPattern(regexPattern);
        searchPattern.setPatternOptions(options);
    }

    const CPlusPlus::Snapshot snapshot = modelManager->snapshot();
    CPlusPlus::Overview overview;

    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        CPlusPlus::Document::Ptr doc = it.value();
        if (!doc || !doc->globalNamespace()) {
            continue;
        }

        QString filePath = doc->filePath().toUserOutput();

        auto project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath));
        if (project && m_ignoreManager->shouldIgnore(filePath, project)) {
            continue;
        }

        searchInScope(
            doc->globalNamespace(),
            symbolName,
            type,
            scopeFilter,
            filePath,
            overview,
            QString(),
            caseSensitive,
            useRegex,
            useWildcard,
            useRegex || useWildcard ? &searchPattern : nullptr,
            results);
    }

    return results;
}

void FindSymbolTool::searchInScope(
    CPlusPlus::Scope *scope,
    const QString &symbolName,
    SymbolType searchType,
    const QString &scopeFilter,
    const QString &filePath,
    const CPlusPlus::Overview &overview,
    const QString &currentScope,
    bool caseSensitive,
    bool useRegex,
    bool useWildcard,
    QRegularExpression *searchPattern,
    QList<SymbolInfo> &results) const
{
    if (!scope) {
        return;
    }

    for (unsigned i = 0; i < scope->memberCount(); ++i) {
        CPlusPlus::Symbol *symbol = scope->memberAt(i);
        if (!symbol || !symbol->name()) {
            continue;
        }

        QString currentSymbolName = overview.prettyName(symbol->name());
        QString fullScope = buildFullScope(currentScope, currentSymbolName);

        if (matchesSymbolName(currentSymbolName,
                              symbolName,
                              caseSensitive,
                              useRegex,
                              useWildcard,
                              searchPattern)
            && matchesType(symbol, searchType)) {
            if (scopeFilter.isEmpty() || matchesScopeFilter(currentScope, scopeFilter)) {
                results.append(createSymbolInfo(symbol, filePath, currentScope, overview));
            }
        }

        if (symbol->asNamespace() || symbol->asClass() || symbol->asEnum()) {
            searchInScope(symbol->asScope(),
                          symbolName,
                          searchType,
                          scopeFilter,
                          filePath,
                          overview,
                          fullScope,
                          caseSensitive,
                          useRegex,
                          useWildcard,
                          searchPattern,
                          results);
        }
    }
}

bool FindSymbolTool::matchesScopeFilter(const QString &fullScope, const QString &scopeFilter) const
{
    if (scopeFilter.isEmpty()) {
        return true;
    }

    // Match if the full scope contains the filter
    // E.g., "MyNamespace::MyClass" matches filter "MyNamespace" or "MyClass"
    return fullScope.contains(scopeFilter) || fullScope.endsWith(scopeFilter);
}

QString FindSymbolTool::buildFullScope(const QString &currentScope, const QString &symbolName) const
{
    if (currentScope.isEmpty()) {
        return symbolName;
    }
    return currentScope + "::" + symbolName;
}

bool FindSymbolTool::matchesSymbolName(
    const QString &symbolName,
    const QString &searchPattern,
    bool caseSensitive,
    bool useRegex,
    bool useWildcard,
    QRegularExpression *regex) const
{
    if (useRegex || useWildcard) {
        // Use regex pattern matching
        if (regex && regex->isValid()) {
            return regex->match(symbolName).hasMatch();
        }
        return false;
    }

    // Exact match (with optional case sensitivity)
    if (caseSensitive) {
        return symbolName == searchPattern;
    } else {
        return symbolName.compare(searchPattern, Qt::CaseInsensitive) == 0;
    }
}

bool FindSymbolTool::matchesType(CPlusPlus::Symbol *symbol, SymbolType type) const
{
    if (type == SymbolType::All) {
        return true;
    }

    switch (type) {
    case SymbolType::Class:
        return symbol->asClass() != nullptr;
    case SymbolType::Function:
        return symbol->asFunction() != nullptr;
    case SymbolType::Enum:
        return symbol->asEnum() != nullptr;
    case SymbolType::Namespace:
        return symbol->asNamespace() != nullptr;
    case SymbolType::Variable:
        return symbol->asDeclaration() != nullptr && !symbol->type()->asFunctionType();
    case SymbolType::Typedef:
        return symbol->asTypenameArgument() != nullptr
               || (symbol->asDeclaration() && symbol->asDeclaration()->isTypedef());
    default:
        return false;
    }
}

FindSymbolTool::SymbolInfo FindSymbolTool::createSymbolInfo(
    CPlusPlus::Symbol *symbol,
    const QString &filePath,
    const QString &fullScope,
    const CPlusPlus::Overview &overview) const
{
    SymbolInfo info;
    info.name = overview.prettyName(symbol->name());
    info.filePath = filePath;
    info.line = symbol->line();
    info.scope = fullScope;

    // Build qualified name
    if (fullScope.isEmpty()) {
        info.qualifiedName = info.name;
    } else {
        info.qualifiedName = fullScope + "::" + info.name;
    }

    // Determine symbol type and extract additional information
    if (symbol->asClass()) {
        info.type = SymbolType::Class;
        info.typeString = "Class";
        info.endLine = findSymbolEndLine(filePath, symbol->line(), SymbolType::Class);
    } else if (auto *function = symbol->asFunction()) {
        info.type = SymbolType::Function;
        info.typeString = "Function";
        info.signature = overview.prettyType(symbol->type());
        info.isConst = function->isConst();
        info.isStatic = function->isStatic();
        info.isVirtual = function->isVirtual();
        info.endLine = findSymbolEndLine(filePath, symbol->line(), SymbolType::Function);
    } else if (symbol->asEnum()) {
        info.type = SymbolType::Enum;
        info.typeString = "Enum";
        info.endLine = findSymbolEndLine(filePath, symbol->line(), SymbolType::Enum);
    } else if (symbol->asNamespace()) {
        info.type = SymbolType::Namespace;
        info.typeString = "Namespace";
        info.endLine = symbol->line(); // Namespaces can span multiple files
    } else if (auto *declaration = symbol->asDeclaration()) {
        if (declaration->isTypedef()) {
            info.type = SymbolType::Typedef;
            info.typeString = "Typedef";
            info.signature = overview.prettyType(symbol->type());
        } else {
            info.type = SymbolType::Variable;
            info.typeString = "Variable";
            info.signature = overview.prettyType(symbol->type());
            info.isStatic = declaration->isStatic();
        }
        info.endLine = symbol->line();
    } else {
        info.typeString = "Symbol";
        info.endLine = symbol->line();
    }

    return info;
}

int FindSymbolTool::findSymbolEndLine(const QString &filePath, int startLine, SymbolType type) const
{
    // For simple types, just return the start line
    if (type == SymbolType::Variable || type == SymbolType::Typedef
        || type == SymbolType::Namespace) {
        return startLine;
    }

    // For classes, enums, and functions, find the closing brace
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return startLine;
    }

    QTextStream stream(&file);
    int currentLine = 1;

    // Skip to start line
    while (currentLine < startLine && !stream.atEnd()) {
        stream.readLine();
        currentLine++;
    }

    bool foundOpenBrace = false;
    int braceCount = 0;

    while (!stream.atEnd()) {
        QString line = stream.readLine();

        for (QChar ch : line) {
            if (ch == '{') {
                foundOpenBrace = true;
                braceCount++;
            } else if (ch == '}') {
                braceCount--;
            }
        }

        if (foundOpenBrace && braceCount == 0) {
            file.close();
            return currentLine;
        }

        // For function declarations without body (e.g., in header), stop at semicolon
        if (type == SymbolType::Function && !foundOpenBrace && line.contains(';')) {
            file.close();
            return currentLine;
        }

        currentLine++;
    }

    file.close();
    return startLine;
}

QString FindSymbolTool::extractSymbolCode(const SymbolInfo &info) const
{
    // Try to use TextDocument first (for open files)
    auto *textDocument = TextEditor::TextDocument::textDocumentForFilePath(
        Utils::FilePath::fromString(info.filePath));

    if (textDocument && textDocument->document()) {
        QTextDocument *doc = textDocument->document();

        QTextBlock startBlock = doc->findBlockByNumber(info.line - 1);
        QTextBlock endBlock = doc->findBlockByNumber(info.endLine - 1);

        if (startBlock.isValid() && endBlock.isValid()) {
            int startPos = startBlock.position();
            int endPos = endBlock.position() + endBlock.length();

            QTextCursor cursor(doc);
            cursor.setPosition(startPos);
            cursor.setPosition(endPos, QTextCursor::KeepAnchor);

            return cursor.selectedText().replace(QChar::ParagraphSeparator, '\n').trimmed();
        }
    }

    // Fallback to file reading
    return extractCodeFromFile(info.filePath, info.line, info.endLine);
}

QString FindSymbolTool::extractCodeFromFile(const QString &filePath, int startLine, int endLine) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Failed to open file: %1").arg(filePath));
        return QString();
    }

    QTextStream stream(&file);
    QString result;
    int currentLine = 1;

    while (currentLine < startLine && !stream.atEnd()) {
        stream.readLine();
        currentLine++;
    }

    while (currentLine <= endLine && !stream.atEnd()) {
        QString line = stream.readLine();
        result += line + "\n";
        currentLine++;
    }

    if (result.endsWith('\n')) {
        result.chop(1);
    }

    file.close();
    return result;
}

QString FindSymbolTool::formatResults(
    const QList<SymbolInfo> &symbols, bool includeCode, const QString &groupBy) const
{
    QString output = QString("Found %1 symbol(s):\n\n").arg(symbols.size());

    if (groupBy == "file") {
        // Group by file
        QMap<QString, QList<SymbolInfo>> grouped;
        for (const SymbolInfo &info : symbols) {
            grouped[info.filePath].append(info);
        }

        for (auto it = grouped.begin(); it != grouped.end(); ++it) {
            output += QString("File: %1\n").arg(it.key());
            for (const SymbolInfo &info : it.value()) {
                output += formatSymbolInfo(info, includeCode, 2);
            }
            output += "\n";
        }
    } else if (groupBy == "scope") {
        // Group by scope
        QMap<QString, QList<SymbolInfo>> grouped;
        for (const SymbolInfo &info : symbols) {
            QString scopeKey = info.scope.isEmpty() ? "Global" : info.scope;
            grouped[scopeKey].append(info);
        }

        for (auto it = grouped.begin(); it != grouped.end(); ++it) {
            output += QString("Scope: %1\n").arg(it.key());
            for (const SymbolInfo &info : it.value()) {
                output += formatSymbolInfo(info, includeCode, 2);
            }
            output += "\n";
        }
    } else {
        // Group by type (default)
        QMap<SymbolType, QList<SymbolInfo>> grouped;
        for (const SymbolInfo &info : symbols) {
            grouped[info.type].append(info);
        }

        for (auto it = grouped.begin(); it != grouped.end(); ++it) {
            const QList<SymbolInfo> &group = it.value();
            if (!group.isEmpty()) {
                output += QString("%1s:\n").arg(group.first().typeString);
                for (const SymbolInfo &info : group) {
                    output += formatSymbolInfo(info, includeCode, 2);
                }
                output += "\n";
            }
        }
    }

    return output.trimmed();
}

QString FindSymbolTool::formatSymbolInfo(
    const SymbolInfo &info, bool includeCode, int indentLevel) const
{
    QString indent = QString(indentLevel, ' ');
    QString output;

    // Basic information
    output += QString("%1%2:%3 - %4")
                  .arg(indent)
                  .arg(info.filePath)
                  .arg(info.line)
                  .arg(info.qualifiedName);

    // Add signature if available
    if (!info.signature.isEmpty()) {
        output += QString(" : %1").arg(info.signature);
    }

    // Add modifiers
    QStringList modifiers;
    if (info.isStatic)
        modifiers << "static";
    if (info.isVirtual)
        modifiers << "virtual";
    if (info.isConst)
        modifiers << "const";

    if (!modifiers.isEmpty()) {
        output += QString(" [%1]").arg(modifiers.join(", "));
    }

    output += "\n";

    // Add code if requested
    if (includeCode && !info.code.isEmpty()) {
        output += "\n```cpp\n" + info.code + "\n```\n\n";
    }

    return output;
}

} // namespace QodeAssist::Tools
