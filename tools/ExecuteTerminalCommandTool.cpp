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
#include <QTimer>

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
    return getCommandDescription();
}

QJsonObject ExecuteTerminalCommandTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";

    const QString commandDesc = getCommandDescription();

    QJsonObject properties;
    properties["command"] = QJsonObject{
        {"type", "string"},
        {"description", commandDesc}};

    properties["args"] = QJsonObject{
        {"type", "string"},
        {"description", 
         "Optional arguments for the command. Arguments with spaces should be properly quoted. "
         "Example: '--file \"path with spaces.txt\" --verbose'"}};

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
    return LLMCore::ToolPermission::FileSystemRead 
         | LLMCore::ToolPermission::FileSystemWrite 
         | LLMCore::ToolPermission::NetworkAccess;
}

QFuture<QString> ExecuteTerminalCommandTool::executeAsync(const QJsonObject &input)
{
    const QString command = input.value("command").toString().trimmed();
    const QString args = input.value("args").toString().trimmed();

    if (command.isEmpty()) {
        LOG_MESSAGE("ExecuteTerminalCommandTool: Command is empty");
        return QtFuture::makeReadyFuture(QString("Error: Command parameter is required."));
    }

    if (command.length() > MAX_COMMAND_LENGTH) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command too long (%1 chars)")
                        .arg(command.length()));
        return QtFuture::makeReadyFuture(
            QString("Error: Command exceeds maximum length of %1 characters.")
                .arg(MAX_COMMAND_LENGTH));
    }

    if (args.length() > MAX_ARGS_LENGTH) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Arguments too long (%1 chars)")
                        .arg(args.length()));
        return QtFuture::makeReadyFuture(
            QString("Error: Arguments exceed maximum length of %1 characters.")
                .arg(MAX_ARGS_LENGTH));
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

    if (!isCommandSafe(command)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' contains unsafe characters")
                        .arg(command));
#ifdef Q_OS_WIN
        const QString allowedChars = "alphanumeric characters, hyphens, underscores, dots, colons, "
                                     "backslashes, and forward slashes";
#else
        const QString allowedChars = "alphanumeric characters, hyphens, underscores, dots, and slashes";
#endif
        return QtFuture::makeReadyFuture(
            QString("Error: Command '%1' contains potentially dangerous characters. "
                    "Only %2 are allowed.")
                .arg(command)
                .arg(allowedChars));
    }

    if (!args.isEmpty() && !areArgumentsSafe(args)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Arguments contain unsafe patterns: '%1'")
                        .arg(args));
        return QtFuture::makeReadyFuture(
            QString("Error: Arguments contain potentially dangerous patterns (command chaining, "
                    "redirection, or pipe operators)."));
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

    QDir dir(workingDir);
    if (!dir.exists() || !dir.isReadable()) {
        LOG_MESSAGE(
            QString("ExecuteTerminalCommandTool: Working directory '%1' is not accessible")
                .arg(workingDir));
        return QtFuture::makeReadyFuture(
            QString("Error: Working directory '%1' does not exist or is not accessible.")
                .arg(workingDir));
    }

    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Executing command '%1' with args '%2' in '%3'")
                    .arg(command)
                    .arg(args.isEmpty() ? "(no args)" : args)
                    .arg(workingDir));

    auto promise = QSharedPointer<QPromise<QString>>::create();
    QFuture<QString> future = promise->future();
    promise->start();

    QProcess *process = new QProcess();
    process->setWorkingDirectory(workingDir);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    process->setReadChannel(QProcess::StandardOutput);

    QTimer *timeoutTimer = new QTimer();
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(COMMAND_TIMEOUT_MS);
    
    auto outputSize = QSharedPointer<qint64>::create(0);

    QObject::connect(timeoutTimer, &QTimer::timeout, [process, promise, command, args, timeoutTimer]() {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1 %2' timed out after %3ms")
                        .arg(command)
                        .arg(args)
                        .arg(COMMAND_TIMEOUT_MS));
        
        process->terminate();
        
        QTimer::singleShot(1000, process, [process]() {
            if (process->state() == QProcess::Running) {
                LOG_MESSAGE("ExecuteTerminalCommandTool: Forcefully killing process after timeout");
                process->kill();
            }
        });
        
        promise->addResult(QString("Error: Command '%1 %2' timed out after %3 seconds. "
                                   "The process has been terminated.")
                               .arg(command)
                               .arg(args.isEmpty() ? "" : args)
                               .arg(COMMAND_TIMEOUT_MS / 1000));
        promise->finish();
        process->deleteLater();
        timeoutTimer->deleteLater();
    });

    QObject::connect(
        process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, process, promise, command, args, timeoutTimer, outputSize](
            int exitCode, QProcess::ExitStatus exitStatus) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();

            const QByteArray rawOutput = process->readAll();
            *outputSize += rawOutput.size();
            const QString output = sanitizeOutput(QString::fromUtf8(rawOutput), *outputSize);

            const QString fullCommand = args.isEmpty() ? command : QString("%1 %2").arg(command).arg(args);

            if (exitStatus == QProcess::NormalExit) {
                if (exitCode == 0) {
                    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' completed "
                                        "successfully (output size: %2 bytes)")
                                    .arg(fullCommand)
                                    .arg(*outputSize));
                    promise->addResult(
                        QString("Command '%1' executed successfully.\n\nOutput:\n%2")
                            .arg(fullCommand)
                            .arg(output.isEmpty() ? "(no output)" : output));
                } else {
                    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' failed with "
                                        "exit code %2 (output size: %3 bytes)")
                                    .arg(fullCommand)
                                    .arg(exitCode)
                                    .arg(*outputSize));
                    promise->addResult(
                        QString("Command '%1' failed with exit code %2.\n\nOutput:\n%3")
                            .arg(fullCommand)
                            .arg(exitCode)
                            .arg(output.isEmpty() ? "(no output)" : output));
                }
            } else {
                LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' crashed or was "
                                    "terminated (output size: %2 bytes)")
                                .arg(fullCommand)
                                .arg(*outputSize));
                const QString error = process->errorString();
                promise->addResult(
                    QString("Command '%1' crashed or was terminated.\n\nError: %2\n\nOutput:\n%3")
                        .arg(fullCommand)
                        .arg(error)
                        .arg(output.isEmpty() ? "(no output)" : output));
            }

            promise->finish();
            process->deleteLater();
        });

    QObject::connect(process, &QProcess::errorOccurred, [process, promise, command, args, timeoutTimer](
                                                             QProcess::ProcessError error) {
        if (promise->future().isFinished()) {
            return;
        }

        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        const QString fullCommand = args.isEmpty() ? command : QString("%1 %2").arg(command).arg(args);
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Process error occurred for '%1': %2 (%3)")
                        .arg(fullCommand)
                        .arg(error)
                        .arg(process->errorString()));
        
        QString errorMessage;
        switch (error) {
        case QProcess::FailedToStart:
            errorMessage = QString("Failed to start command '%1'. The command may not exist or "
                                   "you may not have permission to execute it.")
                               .arg(fullCommand);
            break;
        case QProcess::Crashed:
            errorMessage = QString("Command '%1' crashed during execution.").arg(fullCommand);
            break;
        case QProcess::Timedout:
            errorMessage = QString("Command '%1' timed out.").arg(fullCommand);
            break;
        case QProcess::WriteError:
            errorMessage = QString("Write error occurred while executing '%1'.").arg(fullCommand);
            break;
        case QProcess::ReadError:
            errorMessage = QString("Read error occurred while executing '%1'.").arg(fullCommand);
            break;
        default:
            errorMessage = QString("Unknown error occurred while executing '%1': %2")
                               .arg(fullCommand)
                               .arg(process->errorString());
            break;
        }
        
        promise->addResult(QString("Error: %1").arg(errorMessage));
        promise->finish();
        process->deleteLater();
    });

    QString fullCommand = command;
    if (!args.isEmpty()) {
        fullCommand += " " + args;
    }

#ifdef Q_OS_WIN
    static const QStringList windowsBuiltinCommands = {
        "dir", "type", "del", "copy", "move", "ren", "rename", 
        "md", "mkdir", "rd", "rmdir", "cd", "chdir", "cls", "echo",
        "set", "path", "prompt", "ver", "vol", "date", "time"
    };
    
    const QString lowerCommand = command.toLower();
    const bool isBuiltin = windowsBuiltinCommands.contains(lowerCommand);
    
    if (isBuiltin) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Executing Windows builtin command '%1' via cmd.exe")
                        .arg(command));
        process->start("cmd.exe", QStringList() << "/c" << fullCommand);
    } else {
#endif
        QStringList splitCommand = QProcess::splitCommand(fullCommand);
        if (splitCommand.isEmpty()) {
            LOG_MESSAGE("ExecuteTerminalCommandTool: Failed to parse command");
            promise->addResult(QString("Error: Failed to parse command '%1'").arg(fullCommand));
            promise->finish();
            process->deleteLater();
            timeoutTimer->deleteLater();
            return future;
        }
        
        const QString program = splitCommand.takeFirst();
        process->start(program, splitCommand);
#ifdef Q_OS_WIN
    }
#endif

    if (!process->waitForStarted(PROCESS_START_TIMEOUT_MS)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Failed to start command '%1' within %2ms")
                        .arg(fullCommand)
                        .arg(PROCESS_START_TIMEOUT_MS));
        const QString errorString = process->errorString();
        promise->addResult(QString("Error: Failed to start command '%1': %2\n\n"
                                   "Possible reasons:\n"
                                   "- Command not found in PATH\n"
                                   "- Insufficient permissions\n"
                                   "- Invalid command syntax")
                               .arg(fullCommand)
                               .arg(errorString));
        promise->finish();
        process->deleteLater();
        timeoutTimer->deleteLater();
        return future;
    }

    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Process started successfully (PID: %1)")
                    .arg(process->processId()));

    timeoutTimer->start();
    return future;
}

bool ExecuteTerminalCommandTool::isCommandAllowed(const QString &command) const
{
    const QStringList allowed = getAllowedCommands();
    return allowed.contains(command, Qt::CaseInsensitive);
}

bool ExecuteTerminalCommandTool::isCommandSafe(const QString &command) const
{
#ifdef Q_OS_WIN
    static const QRegularExpression safePattern("^[a-zA-Z0-9._/\\\\:-]+$");
#else
    static const QRegularExpression safePattern("^[a-zA-Z0-9._/-]+$");
#endif
    
    const bool isSafe = safePattern.match(command).hasMatch();
    if (!isSafe) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' failed safety check")
                        .arg(command));
    }
    return isSafe;
}

bool ExecuteTerminalCommandTool::areArgumentsSafe(const QString &args) const
{
    if (args.isEmpty()) {
        return true;
    }

    static const QStringList dangerousPatterns = {
        ";",   // Command separator
        "&&",  // AND operator
        "||",  // OR operator  
        "|",   // Pipe operator
        ">",   // Output redirection
        ">>",  // Append redirection
        "<",   // Input redirection
        "`",   // Command substitution
        "$(",  // Command substitution
        "$()",  // Command substitution
        "\\n", // Newline (could start new command)
        "\\r"  // Carriage return
    };

    for (const QString &pattern : dangerousPatterns) {
        if (args.contains(pattern)) {
            LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Dangerous pattern '%1' found in args")
                            .arg(pattern));
            return false;
        }
    }

    return true;
}

QString ExecuteTerminalCommandTool::sanitizeOutput(const QString &output, qint64 totalSize) const
{
    if (totalSize > MAX_OUTPUT_SIZE) {
        const QString truncated = output.left(MAX_OUTPUT_SIZE / 2);
        return QString("%1\n\n... [Output truncated: exceeded maximum size of %2 MB. "
                       "Total output size was %3 bytes] ...")
            .arg(truncated)
            .arg(MAX_OUTPUT_SIZE / (1024 * 1024))
            .arg(totalSize);
    }

    return output;
}

QStringList ExecuteTerminalCommandTool::getAllowedCommands() const
{
    static QString cachedCommandsStr;
    static QStringList cachedCommands;

    QString commandsStr;

#ifdef Q_OS_LINUX
    commandsStr = Settings::toolsSettings().allowedTerminalCommandsLinux().trimmed();
#elif defined(Q_OS_MACOS)
    commandsStr = Settings::toolsSettings().allowedTerminalCommandsMacOS().trimmed();
#elif defined(Q_OS_WIN)
    commandsStr = Settings::toolsSettings().allowedTerminalCommandsWindows().trimmed();
#else
    commandsStr = Settings::toolsSettings().allowedTerminalCommandsLinux().trimmed(); // fallback
#endif

    if (commandsStr == cachedCommandsStr && !cachedCommands.isEmpty()) {
        return cachedCommands;
    }

    cachedCommandsStr = commandsStr;
    cachedCommands.clear();

    if (commandsStr.isEmpty()) {
        return QStringList();
    }

    const QStringList rawCommands = commandsStr.split(',', Qt::SkipEmptyParts);
    cachedCommands.reserve(rawCommands.size());
    
    for (const QString &cmd : rawCommands) {
        const QString trimmed = cmd.trimmed();
        if (!trimmed.isEmpty()) {
            cachedCommands.append(trimmed);
        }
    }

    return cachedCommands;
}

QString ExecuteTerminalCommandTool::getCommandDescription() const
{
    const QStringList allowed = getAllowedCommands();
    const QString allowedList = allowed.isEmpty() ? "none" : allowed.join(", ");

#ifdef Q_OS_LINUX
    const QString osInfo = " Running on Linux.";
#elif defined(Q_OS_MACOS)
    const QString osInfo = " Running on macOS.";
#elif defined(Q_OS_WIN)
    const QString osInfo = " Running on Windows.";
#else
    const QString osInfo = "";
#endif

    return QString(
               "Execute a terminal command in the project directory. "
               "Only commands from the allowed list can be executed. "
               "Currently allowed commands for this OS: %1. "
               "The command will be executed in the root directory of the active project. "
               "Commands have a %2 second timeout. "
               "Returns the command output (stdout and stderr) or an error message if the command fails.%3")
        .arg(allowedList)
        .arg(COMMAND_TIMEOUT_MS / 1000)
        .arg(osInfo);
}

} // namespace QodeAssist::Tools

