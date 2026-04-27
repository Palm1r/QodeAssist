// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

#include <atomic>

namespace QodeAssist::Tools {

ExecuteTerminalCommandTool::ExecuteTerminalCommandTool(QObject *parent)
    : BaseTool(parent)
{
}

QString ExecuteTerminalCommandTool::id() const
{
    return "execute_terminal_command";
}

QString ExecuteTerminalCommandTool::displayName() const
{
    return "Executing terminal command";
}

QString ExecuteTerminalCommandTool::description() const
{
    return getCommandDescription();
}

QJsonObject ExecuteTerminalCommandTool::parametersSchema() const
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

    return definition;
}

QFuture<LLMQore::ToolResult> ExecuteTerminalCommandTool::executeAsync(const QJsonObject &input)
{
    using LLMQore::ToolResult;

    const QString command = input.value("command").toString().trimmed();
    const QString args = input.value("args").toString().trimmed();

    if (command.isEmpty()) {
        LOG_MESSAGE("ExecuteTerminalCommandTool: Command is empty");
        return QtFuture::makeReadyFuture(ToolResult::error("Error: Command parameter is required."));
    }

    if (command.length() > MAX_COMMAND_LENGTH) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command too long (%1 chars)")
                        .arg(command.length()));
        return QtFuture::makeReadyFuture(
            ToolResult::error(QString("Error: Command exceeds maximum length of %1 characters.")
                .arg(MAX_COMMAND_LENGTH)));
    }

    if (args.length() > MAX_ARGS_LENGTH) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Arguments too long (%1 chars)")
                        .arg(args.length()));
        return QtFuture::makeReadyFuture(
            ToolResult::error(QString("Error: Arguments exceed maximum length of %1 characters.")
                .arg(MAX_ARGS_LENGTH)));
    }

    if (!isCommandAllowed(command)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' is not allowed")
                        .arg(command));
        const QStringList allowed = getAllowedCommands();
        const QString allowedList = allowed.isEmpty() ? "none" : allowed.join(", ");
        return QtFuture::makeReadyFuture(
            ToolResult::error(QString("Error: Command '%1' is not in the allowed list. Allowed commands: %2")
                .arg(command)
                .arg(allowedList)));
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
            ToolResult::error(QString("Error: Command '%1' contains potentially dangerous characters. "
                    "Only %2 are allowed.")
                .arg(command)
                .arg(allowedChars)));
    }

    if (!args.isEmpty() && !areArgumentsSafe(args)) {
        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Arguments contain unsafe patterns: '%1'")
                        .arg(args));
        return QtFuture::makeReadyFuture(
            ToolResult::error(QString("Error: Arguments contain potentially dangerous patterns (command chaining, "
                    "redirection, or pipe operators).")));
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
            ToolResult::error(QString("Error: Working directory '%1' does not exist or is not accessible.")
                .arg(workingDir)));
    }

    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Executing command '%1' with args '%2' in '%3'")
                    .arg(command)
                    .arg(args.isEmpty() ? "(no args)" : args)
                    .arg(workingDir));

    auto promise = QSharedPointer<QPromise<ToolResult>>::create();
    QFuture<ToolResult> future = promise->future();
    promise->start();

    auto resolved = std::make_shared<std::atomic<bool>>(false);

    QProcess *process = new QProcess();
    process->setWorkingDirectory(workingDir);
    process->setProcessChannelMode(QProcess::MergedChannels);

    process->setReadChannel(QProcess::StandardOutput);

    const int timeoutMs = commandTimeoutMs();

    QTimer *timeoutTimer = new QTimer();
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(timeoutMs);

    QObject::connect(timeoutTimer, &QTimer::timeout, [process, promise, resolved, command, args, timeoutTimer, timeoutMs]() {
        if (*resolved)
            return;
        *resolved = true;

        LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1 %2' timed out after %3ms")
                        .arg(command)
                        .arg(args)
                        .arg(timeoutMs));

        process->terminate();

        QTimer::singleShot(1000, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                LOG_MESSAGE("ExecuteTerminalCommandTool: Forcefully killing process after timeout");
                process->kill();
            }
            process->deleteLater();
        });

        promise->addResult(ToolResult::error(QString("Error: Command '%1 %2' timed out after %3 seconds. "
                                   "The process has been terminated.")
                               .arg(command)
                               .arg(args.isEmpty() ? "" : args)
                               .arg(timeoutMs / 1000)));
        promise->finish();
        timeoutTimer->deleteLater();
    });

    QObject::connect(
        process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, process, promise, resolved, command, args, timeoutTimer](
            int exitCode, QProcess::ExitStatus exitStatus) {
            if (*resolved) {
                process->deleteLater();
                return;
            }
            *resolved = true;

            timeoutTimer->stop();
            timeoutTimer->deleteLater();

            const QByteArray rawOutput = process->readAll();
            const qint64 outputSize = rawOutput.size();
            const QString output = sanitizeOutput(QString::fromUtf8(rawOutput), outputSize);

            const QString fullCommand = args.isEmpty() ? command : QString("%1 %2").arg(command).arg(args);

            if (exitStatus == QProcess::NormalExit) {
                if (exitCode == 0) {
                    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' completed "
                                        "successfully (output size: %2 bytes)")
                                    .arg(fullCommand)
                                    .arg(outputSize));
                    promise->addResult(ToolResult::text(
                        QString("Command '%1' executed successfully.\n\nOutput:\n%2")
                            .arg(fullCommand)
                            .arg(output.isEmpty() ? "(no output)" : output)));
                } else {
                    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' failed with "
                                        "exit code %2 (output size: %3 bytes)")
                                    .arg(fullCommand)
                                    .arg(exitCode)
                                    .arg(outputSize));
                    promise->addResult(ToolResult::error(
                        QString("Command '%1' failed with exit code %2.\n\nOutput:\n%3")
                            .arg(fullCommand)
                            .arg(exitCode)
                            .arg(output.isEmpty() ? "(no output)" : output)));
                }
            } else {
                LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Command '%1' crashed or was "
                                    "terminated (output size: %2 bytes)")
                                .arg(fullCommand)
                                .arg(outputSize));
                const QString error = process->errorString();
                promise->addResult(ToolResult::error(
                    QString("Command '%1' crashed or was terminated.\n\nError: %2\n\nOutput:\n%3")
                        .arg(fullCommand)
                        .arg(error)
                        .arg(output.isEmpty() ? "(no output)" : output)));
            }

            promise->finish();
            process->deleteLater();
        });

    QObject::connect(process, &QProcess::errorOccurred, [process, promise, resolved, command, args, timeoutTimer](
                                                             QProcess::ProcessError error) {
        if (*resolved) {
            process->deleteLater();
            return;
        }
        *resolved = true;

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

        promise->addResult(ToolResult::error(QString("Error: %1").arg(errorMessage)));
        promise->finish();
        process->deleteLater();
    });

    QStringList argsList;
    if (!args.isEmpty()) {
        argsList = QProcess::splitCommand(args);
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
        QStringList cmdArgs;
        cmdArgs << "/c" << command;
        cmdArgs.append(argsList);
        process->start("cmd.exe", cmdArgs);
    } else {
        process->start(command, argsList);
    }
#else
    process->start(command, argsList);
#endif

    timeoutTimer->start();

    LOG_MESSAGE(QString("ExecuteTerminalCommandTool: Process start requested for '%1'")
                    .arg(command));

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

    // Check for null bytes
    if (args.contains(QChar('\0'))) {
        LOG_MESSAGE("ExecuteTerminalCommandTool: Null byte found in args");
        return false;
    }

    static const QStringList dangerousPatterns = {
        ";",   // Command separator
        "&",   // Command separator / background execution
        "|",   // Pipe operator
        ">",   // Output redirection
        "<",   // Input redirection
        "`",   // Command substitution
        "$(",  // Command substitution
        "${",  // Variable expansion
        "\n",  // Newline (could start new command)
        "\r",  // Carriage return
#ifdef Q_OS_WIN
        "^",   // Escape character in cmd.exe (can bypass other checks)
        "%",   // Environment variable expansion on Windows
#endif
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

    if (commandsStr.isEmpty()) {
        return QStringList();
    }

    QStringList result;
    const QStringList rawCommands = commandsStr.split(',', Qt::SkipEmptyParts);
    result.reserve(rawCommands.size());

    for (const QString &cmd : rawCommands) {
        const QString trimmed = cmd.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

int ExecuteTerminalCommandTool::commandTimeoutMs() const
{
    return Settings::toolsSettings().terminalCommandTimeout() * 1000;
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
        .arg(commandTimeoutMs() / 1000)
        .arg(osInfo);
}

} // namespace QodeAssist::Tools

