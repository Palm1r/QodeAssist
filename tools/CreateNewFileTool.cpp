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
#include "ToolExceptions.hpp"

#include <logger/Logger.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

CreateNewFileTool::CreateNewFileTool(QObject *parent)
    : BaseTool(parent)
{}

QString CreateNewFileTool::name() const
{
    return "create_new_file";
}

QString CreateNewFileTool::stringName() const
{
    return {"Creating new file"};
}

QString CreateNewFileTool::description() const
{
    return "Create a new empty file at the specified path. "
           "The directory path must exist. Provide absolute file path. After crate files add file "
           "to project file (CMakeLists.txt or .cmake or .pri)";
}

QJsonObject CreateNewFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"] = "The absolute path where the new file should be created";
    properties["filepath"] = filepathProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    QJsonArray required;
    required.append("filepath");
    definition["required"] = required;

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

LLMCore::ToolPermissions CreateNewFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemWrite;
}

QFuture<QString> CreateNewFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString filePath = input["filepath"].toString();

        if (filePath.isEmpty()) {
            throw ToolInvalidArgument("Error: 'filepath' parameter is required");
        }

        QFileInfo fileInfo(filePath);

        if (fileInfo.exists()) {
            throw ToolRuntimeError(
                QString("Error: File already exists at path '%1'").arg(filePath));
        }

        QDir dir = fileInfo.absoluteDir();
        if (!dir.exists()) {
            throw ToolRuntimeError(QString("Error: Directory does not exist: '%1'")
                                       .arg(dir.absolutePath()));
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            throw ToolRuntimeError(
                QString("Error: Could not create file '%1': %2").arg(filePath, file.errorString()));
        }

        file.close();

        LOG_MESSAGE(QString("Successfully created new file: %1").arg(filePath));

        return QString("Successfully created new file: %1").arg(filePath);
    });
}

} // namespace QodeAssist::Tools

