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

#include "EditFileTool.hpp"
#include "ToolExceptions.hpp"

#include <context/ChangesManager.h>
#include <context/ProjectUtils.hpp>
#include <logger/Logger.hpp>
#include <settings/GeneralSettings.hpp>
#include <settings/ToolsSettings.hpp>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtConcurrent>

namespace QodeAssist::Tools {

EditFileTool::EditFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString EditFileTool::name() const
{
    return "edit_file";
}

QString EditFileTool::stringName() const
{
    return {"Editing file"};
}

QString EditFileTool::description() const
{
    return "Edit a file by replacing old content with new content. "
           "Provide the filename (or absolute path), old_content to find and replace, "
           "and new_content to replace it with. Changes are applied immediately if auto-apply "
           "is enabled in settings. The user can undo or reapply changes at any time. "
           "If old_content is empty, new_content will be appended to the end of the file.";
}

QJsonObject EditFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject filenameProperty;
    filenameProperty["type"] = "string";
    filenameProperty["description"]
        = "The filename or absolute path of the file to edit. If only filename is provided, "
          "it will be searched in the project";
    properties["filename"] = filenameProperty;

    QJsonObject oldContentProperty;
    oldContentProperty["type"] = "string";
    oldContentProperty["description"]
        = "The exact content to find and replace. Must match exactly (including whitespace). "
          "If empty, new_content will be appended to the end of the file";
    properties["old_content"] = oldContentProperty;

    QJsonObject newContentProperty;
    newContentProperty["type"] = "string";
    newContentProperty["description"] = "The new content to replace the old content with";
    properties["new_content"] = newContentProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    QJsonArray required;
    required.append("filename");
    required.append("new_content");
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

LLMCore::ToolPermissions EditFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermission::FileSystemWrite;
}

QFuture<QString> EditFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString filename = input["filename"].toString().trimmed();
        QString oldContent = input["old_content"].toString();
        QString newContent = input["new_content"].toString();
        QString requestId = input["_request_id"].toString();

        if (filename.isEmpty()) {
            throw ToolInvalidArgument("'filename' parameter is required and cannot be empty");
        }

        if (newContent.isEmpty()) {
            throw ToolInvalidArgument("'new_content' parameter is required and cannot be empty");
        }


        QString filePath;
        QFileInfo fileInfo(filename);

        if (fileInfo.isAbsolute() && fileInfo.exists()) {
            filePath = filename;
        } else {
            FileSearchUtils::FileMatch match = FileSearchUtils::findBestMatch(
                filename, QString(), 10, m_ignoreManager);

            if (match.absolutePath.isEmpty()) {
                throw ToolRuntimeError(
                    QString("File '%1' not found in project. "
                            "Please provide a valid filename or absolute path.")
                        .arg(filename));
            }

            filePath = match.absolutePath;
            LOG_MESSAGE(QString("EditFileTool: Found file '%1' at '%2'")
                            .arg(filename, filePath));
        }

        QFile file(filePath);
        if (!file.exists()) {
            throw ToolRuntimeError(QString("File does not exist: %1").arg(filePath));
        }

        QFileInfo finalFileInfo(filePath);
        if (!finalFileInfo.isWritable()) {
            throw ToolRuntimeError(
                QString("File is not writable (read-only or permission denied): %1").arg(filePath));
        }

        bool isInProject = Context::ProjectUtils::isFileInProject(filePath);
        if (!isInProject) {
            const auto &settings = Settings::toolsSettings();
            if (!settings.allowAccessOutsideProject()) {
                throw ToolRuntimeError(
                    QString("File path '%1' is not within the current project. "
                            "Enable 'Allow file access outside project' in settings to edit files outside the project.")
                        .arg(filePath));
            }
            LOG_MESSAGE(QString("Editing file outside project scope: %1").arg(filePath));
        }

        QString editId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        bool autoApply = Settings::toolsSettings().autoApplyFileEdits();

        Context::ChangesManager::instance().addFileEdit(
            editId,
            filePath,
            oldContent,
            newContent,
            autoApply,
            false,
            requestId
        );

        auto edit = Context::ChangesManager::instance().getFileEdit(editId);
        QString status = "pending";
        if (edit.status == Context::ChangesManager::Applied) {
            status = "applied";
        } else if (edit.status == Context::ChangesManager::Rejected) {
            status = "rejected";
        } else if (edit.status == Context::ChangesManager::Archived) {
            status = "archived";
        }

        QString statusMessage = edit.statusMessage;

        QJsonObject result;
        result["edit_id"] = editId;
        result["file"] = filePath;
        result["old_content"] = oldContent;
        result["new_content"] = newContent;
        result["status"] = status;
        result["status_message"] = statusMessage;

        LOG_MESSAGE(QString("File edit created: %1 (ID: %2, Status: %3, Deferred: %4)")
                        .arg(filePath, editId, status, requestId.isEmpty() ? QString("no") : QString("yes")));

        QString resultStr = "QODEASSIST_FILE_EDIT:"
                            + QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        return resultStr;
    });
}

} // namespace QodeAssist::Tools

