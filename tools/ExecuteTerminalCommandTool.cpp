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

#include "ExecuteTerminalCommandTool.hpp"

#include <logger/Logger.hpp>
#include <settings/ToolsSettings.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QPromise>
#include <QRegularExpression>
#include <QSharedPointer>

namespace QodeAssist::Tools {

ExecuteTerminalCommandTool::ExecuteTerminalCommandTool(QObject *parent)
    : BaseTool(parent)
{
}

QString ExecuteTerminalCommandTool::name() const
{
    return "execute_terminal_command";
}

QString ExecuteTerminalCommandTool::stringName() const
{
    return "Executing terminal command";
}

QString ExecuteTerminalCommandTool::description() const
{
    const QStringList allowed = getAllowedCommands();
    const QString allowedList = allowed.isEmpty() ? "none" : allowed.join(", ");

    return QString(
               "Execute a terminal command in the project directory. "
               "Only commands from the allowed list can be executed. "
               "Currently allowed commands: %1. "
               "The command will be executed in the root directory of the active project. "
               "Returns the command output (stdout and stderr) or an error message if the command fails.")
        .arg(allowedList);
}

QJsonObject ExecuteTerminalCommandTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    const QStringList allowed = getAllowedCommands();
    const QString allowedList = allowed.isEmpty() ? "none" : allowed.join(", ");

    QJsonObject properties;
    properties["command"] = QJsonObject{
        {"type", "string"},
        {"description",
         QString("The terminal command to execute. Must be one of the allowed commands: %1")
             .arg(allowedList)}};

    properties["args"] = QJsonObject{
        {"type", "string"},
        {"description", "Optional arguments for the command (default: empty)"}};

    definition["properties"] = properties;
    definition["required"] = QJsonArray{"command"};

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

LLMCore::ToolPermissions ExecuteTerminalCommandTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::None;
}

QFuture<QString> ExecuteTerminalCommandTool::executeAsync(const QJsonObject &input)
{
    const QString command = input.value("command").toString().trimmed();
    const QString args = input.value("args").toString().trimmed();

    if (command.isEmpty()) {
        LOG_MESSAGE("ExecuteTerminalCommandTool: Command is empty");
        return QtFuture::makeReadyFuture(QString("Error: Command parameter is required."));
    }

    if (!isCommandAllowed(command)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' is not allowed")
                        .arg(command));
        const QStringList allowed = getAllowedCommands();
        const QString allowedList = allowed.isEmpty() ? "none" : allowed.join(", ");
        return QtFuture::makeReadyFuture(
            QString("Error: Command '%1' is not in the allowed list. Allowed commands: %2")
                .arg(command)
                .arg(allowedList));
    }

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    QString workingDir;

    if (project) {
        workingDir = project->projectDirectory().toString();
        LOG_MESSAGE(
            QString("ExecuteTerminalCommandTool: Working directory is '%1'").arg(workingDir));
    } else {
        LOG_MESSAGE("ExecuteTerminalCommandTool: No active project, using current directory");
        workingDir = QDir::currentPath();
    }

    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Executing command '%1' with args '%2'")
                    .arg(command)
                    .arg(args));

    auto promise = QSharedPointer<QPromise<QString>>::create();
    QFuture<QString> future = promise->future();
    promise->start();

    QProcess *process = new QProcess();
    process->setWorkingDirectory(workingDir);
    process->setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(
        process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [process, promise, command, args](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output = QString::fromUtf8(process->readAll());

            if (exitStatus == QProcess::NormalExit) {
                if (exitCode == 0) {
                    LOG_MESSAGE(
                        QString("ExecuteTerminalCommandTool: Command completed successfully"));
                    promise->addResult(
                        QString("Command '%1 %2' executed successfully.\n\nOutput:\n%3")
                            .arg(command)
                            .arg(args)
                            .arg(output.isEmpty() ? "(no output)" : output));
                } else {
                    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command failed with exit "
                                        "code %1")
                                    .arg(exitCode));
                    promise->addResult(
                        QString("Command '%1 %2' failed with exit code %3.\n\nOutput:\n%4")
                            .arg(command)
                            .arg(args)
                            .arg(exitCode)
                            .arg(output.isEmpty() ? "(no output)" : output));
                }
            } else {
                LOG_MESSAGE("ExecuteTerminalCommandTool: Command crashed or was terminated");
                const QString error = process->errorString();
                promise->addResult(
                    QString("Command '%1 %2' crashed or was terminated.\n\nError: %3\n\nOutput:\n%4")
                        .arg(command)
                        .arg(args)
                        .arg(error)
                        .arg(output.isEmpty() ? "(no output)" : output));
            }

            promise->finish();
            process->deleteLater();
        });

    QObject::connect(process, &QProcess::errorOccurred, [process, promise, command, args](
                                                             QProcess::ProcessError error) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Process error occurred: %1").arg(error));
        const QString errorString = process->errorString();
        promise->addResult(QString("Error executing command '%1 %2': %3")
                               .arg(command)
                               .arg(args)
                               .arg(errorString));
        promise->finish();
        process->deleteLater();
    });

    if (args.isEmpty()) {
        process->start(command, QStringList());
    } else {
        QStringList argList = args.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        process->start(command, argList);
    }

    return future;
}

bool ExecuteTerminalCommandTool::isCommandAllowed(const QString &command) const
{
    const QStringList allowed = getAllowedCommands();
    return allowed.contains(command, Qt::CaseInsensitive);
}

QStringList ExecuteTerminalCommandTool::getAllowedCommands() const
{
    const QString commandsStr
        = Settings::toolsSettings().allowedTerminalCommands().trimmed();

    if (commandsStr.isEmpty()) {
        return QStringList();
    }

    QStringList commands = commandsStr.split(',', Qt::SkipEmptyParts);
    for (QString &cmd : commands) {
        cmd = cmd.trimmed();
    }

    return commands;
}

} // namespace QodeAssist::Tools

