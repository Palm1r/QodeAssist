// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ReadFileTool.hpp"

#include "FileSearchUtils.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <context/ProjectUtils.hpp>
#include <logger/Logger.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ReadFileTool::ReadFileTool(QObject *parent)
    : BaseTool(parent)
{}

QString ReadFileTool::id() const
{
    return "read_file";
}

QString ReadFileTool::displayName() const
{
    return "Reading file";
}

QString ReadFileTool::description() const
{
    return "Read the text content of a file. Accepts either an absolute path or a path "
           "relative to the project root. Reading files outside the active project requires "
           "the 'Allow file access outside project' option in settings. Use `find_file` first "
           "if you only know a partial filename.";
}

QJsonObject ReadFileTool::parametersSchema() const
{
    QJsonObject properties;

    properties["file_path"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Absolute path (e.g. /path/to/file.cpp) or project-relative path (e.g. src/main.cpp). "
         "Relative paths are resolved against the root of the active project."}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"file_path"};

    return definition;
}

QFuture<LLMQore::ToolResult> ReadFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([input]() -> LLMQore::ToolResult {
        QString rawPath = input["file_path"].toString().trimmed();
        if (rawPath.isEmpty()) {
            throw LLMQore::ToolInvalidArgument(
                "'file_path' parameter is required and cannot be empty");
        }

        QFileInfo pathInfo(rawPath);
        QString absolutePath;
        if (pathInfo.isAbsolute()) {
            absolutePath = rawPath;
        } else {
            QString projectRoot = Context::ProjectUtils::getProjectRoot();
            if (projectRoot.isEmpty()) {
                throw LLMQore::ToolRuntimeError(
                    QString("Cannot resolve relative path '%1': no project is open. "
                            "Provide an absolute path or open a project.")
                        .arg(rawPath));
            }
            absolutePath = QDir(projectRoot).absoluteFilePath(rawPath);
            LOG_MESSAGE(QString("ReadFileTool: Resolved relative path '%1' to '%2'")
                            .arg(rawPath, absolutePath));
        }

        QFileInfo finalInfo(absolutePath);
        if (!finalInfo.exists() || !finalInfo.isFile()) {
            throw LLMQore::ToolRuntimeError(QString("File does not exist: %1").arg(absolutePath));
        }

        QString content = FileSearchUtils::readFileContent(absolutePath);
        if (content.isNull()) {
            throw LLMQore::ToolRuntimeError(
                QString("Cannot read file '%1'. It may be outside the project scope "
                        "(enable 'Allow file access outside project' in settings), unreadable, "
                        "or use an unsupported encoding.")
                    .arg(absolutePath));
        }

        QString result = QString("File: %1\n\n%2").arg(absolutePath, content);
        return LLMQore::ToolResult::text(result);
    });
}

} // namespace QodeAssist::Tools
