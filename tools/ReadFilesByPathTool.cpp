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

#include "ReadFilesByPathTool.hpp"
#include "ToolExceptions.hpp"

#include <context/ProjectUtils.hpp>
#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <settings/GeneralSettings.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ReadFilesByPathTool::ReadFilesByPathTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString ReadFilesByPathTool::name() const
{
    return "read_files_by_path";
}

QString ReadFilesByPathTool::stringName() const
{
    return {"Reading file(s)"};
}

QString ReadFilesByPathTool::description() const
{
    return "Read content of project file(s) by absolute path. "
           "Use 'filepath' for single file or 'filepaths' array for multiple files (e.g., .h and .cpp). "
           "Files must exist and not be excluded by .qodeassistignore.";
}

QJsonObject ReadFilesByPathTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"] = "The absolute file path to read (for single file)";
    properties["filepath"] = filepathProperty;

    QJsonObject filepathsProperty;
    filepathsProperty["type"] = "array";
    QJsonObject itemsProperty;
    itemsProperty["type"] = "string";
    filepathsProperty["items"] = itemsProperty;
    filepathsProperty["description"] = "Array of absolute file paths to read (for multiple files, "
                                       "e.g., both .h and .cpp)";
    properties["filepaths"] = filepathsProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["description"] = "Provide either 'filepath' for a single file or 'filepaths' for "
                                "multiple files";

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

LLMCore::ToolPermissions ReadFilesByPathTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemRead;
}

QFuture<QString> ReadFilesByPathTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QStringList filePaths;

        QString singlePath = input["filepath"].toString();
        if (!singlePath.isEmpty()) {
            filePaths.append(singlePath);
        }

        if (input.contains("filepaths") && input["filepaths"].isArray()) {
            QJsonArray pathsArray = input["filepaths"].toArray();
            for (const auto &pathValue : pathsArray) {
                QString path = pathValue.toString();
                if (!path.isEmpty()) {
                    filePaths.append(path);
                }
            }
        }

        if (filePaths.isEmpty()) {
            QString error = "Error: either 'filepath' or 'filepaths' parameter is required";
            throw ToolInvalidArgument(error);
        }

        LOG_MESSAGE(QString("Processing %1 file(s)").arg(filePaths.size()));

        QList<FileResult> results;
        for (const QString &filePath : filePaths) {
            results.append(processFile(filePath));
        }

        return formatResults(results);
    });
}

QString ReadFilesByPathTool::readFileContent(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file: %1, error: %2").arg(filePath, file.errorString()));
        throw ToolRuntimeError(
            QString("Error: Could not open file '%1': %2").arg(filePath, file.errorString()));
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();

    file.close();

    LOG_MESSAGE(QString("Successfully read file: %1, size: %2 bytes, isEmpty: %3")
                    .arg(filePath)
                    .arg(content.length())
                    .arg(content.isEmpty()));

    return content;
}

ReadFilesByPathTool::FileResult ReadFilesByPathTool::processFile(const QString &filePath) const
{
    FileResult result;
    result.path = filePath;
    result.success = false;

    try {
        QFileInfo fileInfo(filePath);
        LOG_MESSAGE(QString("Checking file: %1, exists: %2, isFile: %3")
                        .arg(filePath)
                        .arg(fileInfo.exists())
                        .arg(fileInfo.isFile()));

        if (!fileInfo.exists() || !fileInfo.isFile()) {
            result.error = QString("File does not exist");
            return result;
        }

        QString canonicalPath = fileInfo.canonicalFilePath();
        LOG_MESSAGE(QString("Canonical path: %1").arg(canonicalPath));

        bool isInProject = Context::ProjectUtils::isFileInProject(canonicalPath);

        if (!isInProject) {
            const auto &settings = Settings::generalSettings();
            if (!settings.allowAccessOutsideProject()) {
                result.error = QString(
                    "File is not part of the project. "
                    "Enable 'Allow file access outside project' in settings "
                    "to read files outside project scope.");
                return result;
            }
            LOG_MESSAGE(QString("Reading file outside project scope: %1").arg(canonicalPath));
        }

        auto project = isInProject ? ProjectExplorer::ProjectManager::projectForFile(
                                         Utils::FilePath::fromString(canonicalPath))
                                   : nullptr;
        if (isInProject && project && m_ignoreManager->shouldIgnore(canonicalPath, project)) {
            result.error = QString("File is excluded by .qodeassistignore");
            return result;
        }

        result.content = readFileContent(canonicalPath);
        result.success = true;
        result.path = canonicalPath;

    } catch (const ToolRuntimeError &e) {
        result.error = e.message();
    } catch (const std::exception &e) {
        result.error = QString("Unexpected error: %1").arg(QString::fromUtf8(e.what()));
    }

    return result;
}

QString ReadFilesByPathTool::formatResults(const QList<FileResult> &results) const
{
    if (results.size() == 1) {
        const FileResult &result = results.first();
        if (!result.success) {
            throw ToolRuntimeError(QString("Error: %1 - %2").arg(result.path, result.error));
        }

        if (result.content.isEmpty()) {
            return QString("File: %1\n\nThe file is empty").arg(result.path);
        }

        return QString("File: %1\n\nContent:\n%2").arg(result.path, result.content);
    }

    QStringList output;
    int successCount = 0;

    for (const FileResult &result : results) {
        if (result.success) {
            successCount++;
            output.append(QString("=== File: %1 ===\n").arg(result.path));

            if (result.content.isEmpty()) {
                output.append("[Empty file]\n");
            } else {
                output.append(result.content);
            }
            output.append("\n");
        } else {
            output.append(QString("=== File: %1 ===\n").arg(result.path));
            output.append(QString("[Error: %1]\n\n").arg(result.error));
        }
    }

    QString summary
        = QString("Successfully read %1 of %2 file(s)\n\n").arg(successCount).arg(results.size());

    return summary + output.join("");
}

} // namespace QodeAssist::Tools
