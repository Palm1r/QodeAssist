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

#include "CreateNewFileTool.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <context/ProjectUtils.hpp>
#include <logger/Logger.hpp>
#include <settings/GeneralSettings.hpp>
#include <settings/ToolsSettings.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

CreateNewFileTool::CreateNewFileTool(QObject *parent)
    : BaseTool(parent)
{}

QString CreateNewFileTool::id() const
{
    return "create_new_file";
}

QString CreateNewFileTool::displayName() const
{
    return {"Creating new file"};
}

QString CreateNewFileTool::description() const
{
    return "Create a new empty file at the given absolute path. Any missing parent directories "
           "are created automatically. The file is written to disk only — it is NOT added to the "
           "project's build system automatically; the user must register it in CMakeLists.txt or "
           "the equivalent project file. Use `edit_file` afterwards to populate its content.";
}

QJsonObject CreateNewFileTool::parametersSchema() const
{
    QJsonObject properties;

    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"]
        = "Absolute path where the new file should be created. Parent directories are made if "
          "missing. Relative paths are rejected.";
    properties["filepath"] = filepathProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    QJsonArray required;
    required.append("filepath");
    definition["required"] = required;

    return definition;
}

QFuture<LLMQore::ToolResult> CreateNewFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> LLMQore::ToolResult {
        QString filePath = input["filepath"].toString();

        if (filePath.isEmpty()) {
            throw LLMQore::ToolInvalidArgument("Error: 'filepath' parameter is required");
        }

        QFileInfo fileInfo(filePath);
        QString absolutePath = fileInfo.absoluteFilePath();

        bool isInProject = Context::ProjectUtils::isFileInProject(absolutePath);

        if (!isInProject) {
            const auto &settings = Settings::toolsSettings();
            if (!settings.allowAccessOutsideProject()) {
                throw LLMQore::ToolRuntimeError(
                    QString("Error: File path '%1' is not within the current project. "
                            "Enable 'Allow file access outside project' in settings to create files outside project scope.")
                        .arg(absolutePath));
            }
            LOG_MESSAGE(QString("Creating file outside project scope: %1").arg(absolutePath));
        }

        if (fileInfo.exists()) {
            throw LLMQore::ToolRuntimeError(
                QString("Error: File already exists at path '%1'").arg(filePath));
        }

        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                throw LLMQore::ToolRuntimeError(
                    QString("Error: Could not create directory: '%1'").arg(dir.absolutePath()));
            }
            LOG_MESSAGE(QString("Created directory path: %1").arg(dir.absolutePath()));
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::WriteOnly)) {
            throw LLMQore::ToolRuntimeError(
                QString("Error: Could not create file '%1': %2").arg(absolutePath, file.errorString()));
        }

        file.close();

        LOG_MESSAGE(QString("Successfully created new file: %1").arg(absolutePath));

        return LLMQore::ToolResult::text(QString("Successfully created new file: %1").arg(absolutePath));
    });
}

} // namespace QodeAssist::Tools

