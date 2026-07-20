// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <LLMQore/BaseTool.hpp>

namespace QodeAssist::Tools {

class ListOpenEditorsTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit ListOpenEditorsTool(QObject *parent = nullptr);

    void setIgnorePredicate(std::function<bool(const QString &)> predicate);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    ::LLMQore::ToolSafety safety() const override { return ::LLMQore::ToolSafety::ReadOnly; }

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    std::function<bool(const QString &)> m_ignorePredicate;
};

class GetEditorSelectionTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit GetEditorSelectionTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    ::LLMQore::ToolSafety safety() const override { return ::LLMQore::ToolSafety::ReadOnly; }

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;
};

class GetProjectModelTool : public ::LLMQore::BaseTool
{
    Q_OBJECT

public:
    explicit GetProjectModelTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;
    ::LLMQore::ToolSafety safety() const override { return ::LLMQore::ToolSafety::ReadOnly; }

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;
};

} // namespace QodeAssist::Tools
