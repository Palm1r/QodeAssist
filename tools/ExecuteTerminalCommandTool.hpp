// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/BaseTool.hpp>
#include <QObject>

namespace QodeAssist::Tools {

class ExecuteTerminalCommandTool : public ::LLMQore::BaseTool
{
    Q_OBJECT
public:
    explicit ExecuteTerminalCommandTool(QObject *parent = nullptr);

    QString id() const override;
    QString displayName() const override;
    QString description() const override;
    QJsonObject parametersSchema() const override;

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    bool isCommandAllowed(const QString &command) const;
    bool isCommandSafe(const QString &command) const;
    bool areArgumentsSafe(const QString &args) const;
    QStringList getAllowedCommands() const;
    QString getCommandDescription() const;
    QString sanitizeOutput(const QString &output, qint64 maxSize) const;
    
    int commandTimeoutMs() const;

    // Constants for production safety
    static constexpr qint64 MAX_OUTPUT_SIZE = 10 * 1024 * 1024; // 10 MB
    static constexpr int MAX_COMMAND_LENGTH = 1024;
    static constexpr int MAX_ARGS_LENGTH = 4096;
};

} // namespace QodeAssist::Tools

