// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <context/IgnoreManager.hpp>
#include <LLMQore/BaseTool.hpp>
#include <QFuture>
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::Tools {

class ProjectSearchTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit ProjectSearchTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override;

private:
    enum class SearchType { Text, Symbol };
    enum class SymbolType { All, Class, Function, Enum, Variable, Namespace };

    struct SearchResult
    {
        QString filePath;
        QString relativePath;
        QString content;
        int lineNumber = 0;
        QString context;
    };

    QList<SearchResult> searchText(
        const QString &query,
        bool caseSensitive,
        bool useRegex,
        bool wholeWords,
        const QString &filePattern);

    QList<SearchResult> searchSymbols(
        const QString &query, SymbolType symbolType, bool caseSensitive, bool useRegex);

    SymbolType parseSymbolType(const QString &typeStr);
    QString formatResults(const QList<SearchResult> &results, const QString &query);

    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
