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

#include "ReadVisibleFilesTool.hpp"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <logger/Logger.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>

namespace QodeAssist::Tools {

ReadVisibleFilesTool::ReadVisibleFilesTool(QObject *parent)
    : BaseTool(parent)
{}

QString ReadVisibleFilesTool::name() const
{
    return "read_visible_files";
}

QString ReadVisibleFilesTool::description() const
{
    return "Read the content of all currently visible files in editor tabs. "
           "Returns content from all open tabs that are currently visible, including unsaved "
           "changes. "
           "No parameters required.";
}

QJsonObject ReadVisibleFilesTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = QJsonObject();
    definition["required"] = QJsonArray();

    switch (format) {
    case LLMCore::ToolSchemaFormat::OpenAI:
        return customizeForOpenAI(definition);
    case LLMCore::ToolSchemaFormat::Claude:
        return customizeForClaude(definition);
    case LLMCore::ToolSchemaFormat::Ollama:
        return customizeForOllama(definition);
    }

    return definition;
}

QFuture<QString> ReadVisibleFilesTool::executeAsync(const QJsonObject &input)
{
    Q_UNUSED(input)

    return QtConcurrent::run([this]() -> QString {
        auto editors = Core::EditorManager::visibleEditors();

        if (editors.isEmpty()) {
            QString error = "Error: No visible files in the editor";
            throw std::runtime_error(error.toStdString());
        }

        QStringList results;

        for (auto editor : editors) {
            if (!editor || !editor->document()) {
                continue;
            }

            QString filePath = editor->document()->filePath().toFSPathString();
            QByteArray contentBytes = editor->document()->contents();
            QString fileContent = QString::fromUtf8(contentBytes);

            QString fileResult;
            if (fileContent.isEmpty()) {
                fileResult
                    = QString("File: %1\n\nThe file is empty or could not be read").arg(filePath);
            } else {
                fileResult = QString("File: %1\n\nContent:\n%2").arg(filePath, fileContent);
            }

            results.append(fileResult);
        }

        return results.join("\n\n" + QString(80, '=') + "\n\n");
    });
}

} // namespace QodeAssist::Tools
