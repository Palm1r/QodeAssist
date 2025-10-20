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

#pragma once

#include <context/IgnoreManager.hpp>
#include <llmcore/BaseTool.hpp>
#include <QSharedPointer>

namespace CPlusPlus {
class Symbol;
class Scope;
class Overview;
class Document;
} // namespace CPlusPlus

namespace QodeAssist::Tools {

class FindSymbolTool : public LLMCore::BaseTool
{
    Q_OBJECT
public:
    explicit FindSymbolTool(QObject *parent = nullptr);

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;

    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    enum class SymbolType { All, Class, Function, Enum, Variable, Typedef, Namespace };

    struct SymbolInfo
    {
        QString name;
        QString qualifiedName;
        QString filePath;
        int line;
        int endLine;
        QString scope;
        SymbolType type;
        QString typeString;
        QString signature;
        QString code;
        bool isConst = false;
        bool isStatic = false;
        bool isVirtual = false;
    };

    QList<SymbolInfo> findSymbols(
        const QString &symbolName,
        SymbolType type,
        const QString &scopeFilter,
        bool caseSensitive,
        bool useRegex,
        bool useWildcard) const;
    QString formatResults(
        const QList<SymbolInfo> &symbols,
        bool includeCode,
        const QString &groupBy) const;
    SymbolType parseSymbolType(const QString &typeStr) const;

    void searchInScope(
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
        QList<SymbolInfo> &results) const;

    bool matchesType(CPlusPlus::Symbol *symbol, SymbolType type) const;
    bool matchesScopeFilter(const QString &fullScope, const QString &scopeFilter) const;
    bool matchesSymbolName(
        const QString &symbolName,
        const QString &searchPattern,
        bool caseSensitive,
        bool useRegex,
        bool useWildcard,
        QRegularExpression *regex) const;

    SymbolInfo createSymbolInfo(
        CPlusPlus::Symbol *symbol,
        const QString &filePath,
        const QString &fullScope,
        const CPlusPlus::Overview &overview) const;

    QString extractSymbolCode(const SymbolInfo &info) const;
    QString extractCodeFromFile(const QString &filePath, int startLine, int endLine) const;
    int findSymbolEndLine(const QString &filePath, int startLine, SymbolType type) const;
    QString buildFullScope(const QString &currentScope, const QString &symbolName) const;
    QString formatSymbolInfo(const SymbolInfo &info, bool includeCode, int indentLevel) const;

    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
