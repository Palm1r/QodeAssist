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

