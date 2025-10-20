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

#include "EditProjectFileTool.hpp"

#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTextStream>
#include <QtConcurrent>

namespace QodeAssist::Tools {

EditProjectFileTool::EditProjectFileTool(QObject *parent)
    : BaseTool(parent)
    , m_ignoreManager(new Context::IgnoreManager(this))
{}

QString EditProjectFileTool::name() const
{
    return "edit_project_file";
}

QString EditProjectFileTool::stringName() const
{
    return {"Editing project file"};
}

QString EditProjectFileTool::description() const
{
    return "Edit the content of a specific file in the current project. This tool proposes file "
           "changes that will be shown to the user for approval.\n\n"
           "**Edit Modes (choose the most precise mode for the change):**\n"
           "1. 'replace' - Replace exact multi-line text blocks (use for substantial changes)\n"
           "2. 'insert_before' - Insert new lines before a specific line number (preferred for "
           "adding imports, comments, or new code)\n"
           "3. 'insert_after' - Insert new lines after a specific line number (preferred for "
           "adding code after existing lines)\n"
           "4. 'append' - Append new content to the end of file\n\n"
           "**Best Practices:**\n"
           "- For single-line changes: use 'replace' mode with exact line content\n"
           "- For adding new lines: prefer 'insert_before' or 'insert_after' over 'replace'\n"
           "- For adding imports/includes: use 'insert_after' at the end of import section\n"
           "- For multi-line refactoring: use 'replace' mode\n"
           "- Keep search_text as small as possible while remaining unique\n\n"
           "**Input Parameters:**\n"
           "- 'filename' (required): Name or relative path of the file to edit\n"
           "- 'mode' (required): Edit mode - 'replace', 'insert_before', 'insert_after', or "
           "'append'\n"
           "- 'search_text' (optional): Exact text to find (required for 'replace' mode)\n"
           "- 'new_text' (required): New text to insert or use as replacement\n"
           "- 'line_number' (optional): Line number for insert operations (required for "
           "'insert_before' and 'insert_after' modes)\n\n"
           "**Usage Examples:**\n"
           "- Single line fix: {\"filename\": \"main.cpp\", \"mode\": \"replace\", "
           "\"search_text\": \"    Test test = new Test();\", \"new_text\": \"    Test test;\"}\n"
           "- Add import: {\"filename\": \"main.cpp\", \"mode\": \"insert_after\", "
           "\"line_number\": 4, \"new_text\": \"#include <memory>\"}\n"
           "- Add function: {\"filename\": \"main.cpp\", \"mode\": \"insert_before\", "
           "\"line_number\": 20, \"new_text\": \"void helper() {\\n    // code\\n}\\n\"}\n\n"
           "**Important Notes:**\n"
           "- Files excluded by .qodeassistignore cannot be edited\n"
           "- Changes will be shown to user with diff for approval before applying\n"
           "- For 'replace' mode, search_text must match exactly (including whitespace)\n"
           "- Line numbers are 1-based\n"
           "- User will see a visual diff and can approve or reject the change";
}

QJsonObject EditProjectFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject filenameProperty;
    filenameProperty["type"] = "string";
    filenameProperty["description"] = "The filename or relative path to edit";
    properties["filename"] = filenameProperty;

    QJsonObject modeProperty;
    modeProperty["type"] = "string";
    modeProperty["description"]
        = "Edit mode: 'replace', 'insert_before', 'insert_after', or 'append'";
    QJsonArray modeEnum;
    modeEnum.append("replace");
    modeEnum.append("insert_before");
    modeEnum.append("insert_after");
    modeEnum.append("append");
    modeProperty["enum"] = modeEnum;
    properties["mode"] = modeProperty;

    QJsonObject searchTextProperty;
    searchTextProperty["type"] = "string";
    searchTextProperty["description"]
        = "Text to search for and replace (required for 'replace' mode)";
    properties["search_text"] = searchTextProperty;

    QJsonObject newTextProperty;
    newTextProperty["type"] = "string";
    newTextProperty["description"] = "New text to insert or use as replacement";
    properties["new_text"] = newTextProperty;

    QJsonObject lineNumberProperty;
    lineNumberProperty["type"] = "integer";
    lineNumberProperty["description"]
        = "Line number for insert operations (1-based, required for insert modes)";
    properties["line_number"] = lineNumberProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("filename");
    required.append("mode");
    required.append("new_text");
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

LLMCore::ToolPermissions EditProjectFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemRead) 
         | LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemWrite);
}

QFuture<QString> EditProjectFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString filename = input["filename"].toString();
        if (filename.isEmpty()) {
            QString error = "Error: filename parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        QString modeStr = input["mode"].toString();
        if (modeStr.isEmpty()) {
            QString error = "Error: mode parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        EditMode mode;
        if (modeStr == "replace") {
            mode = EditMode::Replace;
        } else if (modeStr == "insert_before") {
            mode = EditMode::InsertBefore;
        } else if (modeStr == "insert_after") {
            mode = EditMode::InsertAfter;
        } else if (modeStr == "append") {
            mode = EditMode::AppendToEnd;
        } else {
            QString error = QString("Error: Invalid mode '%1'. Must be one of: replace, "
                                    "insert_before, insert_after, append")
                                .arg(modeStr);
            throw std::invalid_argument(error.toStdString());
        }

        QString newText = input["new_text"].toString();
        if (newText.isEmpty()) {
            QString error = "Error: new_text parameter is required";
            throw std::invalid_argument(error.toStdString());
        }

        QString searchText = input["search_text"].toString();
        if (mode == EditMode::Replace && searchText.isEmpty()) {
            QString error = "Error: search_text parameter is required for replace mode";
            throw std::invalid_argument(error.toStdString());
        }

        int lineNumber = input["line_number"].toInt(0);
        if ((mode == EditMode::InsertBefore || mode == EditMode::InsertAfter) && lineNumber <= 0) {
            QString error = "Error: line_number parameter is required for insert modes and must "
                            "be greater than 0";
            throw std::invalid_argument(error.toStdString());
        }

        QString filePath = findFileInProject(filename);
        if (filePath.isEmpty()) {
            QString error = QString("Error: File '%1' not found in project").arg(filename);
            throw std::runtime_error(error.toStdString());
        }

        auto project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath));
        if (project && m_ignoreManager->shouldIgnore(filePath, project)) {
            QString error
                = QString("Error: File '%1' is excluded by .qodeassistignore and cannot be edited")
                      .arg(filename);
            throw std::runtime_error(error.toStdString());
        }

        QString originalContent = readFileContent(filePath);
        if (originalContent.isNull()) {
            QString error = QString("Error: Could not read file '%1'").arg(filePath);
            throw std::runtime_error(error.toStdString());
        }

        LOG_MESSAGE(QString("Prepared file edit: %1 (mode: %2)").arg(filePath, modeStr));

        QString editId = QString("edit_%1_%2")
                             .arg(QDateTime::currentMSecsSinceEpoch())
                             .arg(QRandomGenerator::global()->generate());

        QString contextBefore, contextAfter;
        extractContext(originalContent, mode, searchText, lineNumber, contextBefore, contextAfter);

        QJsonObject result;
        result["type"] = "file_edit";
        result["edit_id"] = editId;
        result["file_path"] = filePath;
        result["mode"] = modeStr;
        result["original_content"] = (mode == EditMode::Replace) ? searchText : "";
        result["new_content"] = newText;
        result["context_before"] = contextBefore;
        result["context_after"] = contextAfter;
        result["search_text"] = searchText;
        result["line_number"] = lineNumber;

        QJsonDocument doc(result);
        return QString("QODEASSIST_FILE_EDIT:%1")
            .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    });
}

QString EditProjectFileTool::findFileInProject(const QString &fileName) const
{
    QList<ProjectExplorer::Project *> projects = ProjectExplorer::ProjectManager::projects();
    if (projects.isEmpty()) {
        LOG_MESSAGE("No projects found");
        return QString();
    }

    struct FileMatch
    {
        QString path;
        int priority; // 1 = exact filename, 2 = ends with, 3 = contains
    };

    QVector<FileMatch> matches;

    for (auto project : projects) {
        if (!project)
            continue;

        Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);

        for (const auto &projectFile : std::as_const(projectFiles)) {
            QString absolutePath = projectFile.path();

            if (m_ignoreManager->shouldIgnore(absolutePath, project)) {
                continue;
            }

            QString baseName = projectFile.fileName();
            
            if (baseName == fileName) {
                return absolutePath;
            }

            if (projectFile.endsWith(fileName)) {
                matches.append({absolutePath, 2});
            } else if (baseName.contains(fileName, Qt::CaseInsensitive)) {
                matches.append({absolutePath, 3});
            }
        }
    }

    if (!matches.isEmpty()) {
        std::sort(matches.begin(), matches.end(), [](const FileMatch &a, const FileMatch &b) {
            return a.priority < b.priority;
        });
        return matches.first().path;
    }

    return QString();
}

QString EditProjectFileTool::readFileContent(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file for reading: %1").arg(filePath));
        return QString();
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();
    file.close();
    return content;
}

void EditProjectFileTool::extractContext(
    const QString &content,
    EditMode mode,
    const QString &searchText,
    int lineNumber,
    QString &contextBefore,
    QString &contextAfter,
    int contextLines) const
{
    contextBefore.clear();
    contextAfter.clear();

    QStringList lines = content.split('\n');
    int targetLine = -1;

    if (mode == EditMode::Replace && !searchText.isEmpty()) {
        int bestMatch = -1;
        int maxScore = -1;

        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(searchText)) {
                int score = 0;
                for (int offset = 1; offset <= contextLines; ++offset) {
                    if (i - offset >= 0) score++;
                    if (i + offset < lines.size()) score++;
                }
                
                if (score > maxScore) {
                    maxScore = score;
                    bestMatch = i;
                }
            }
        }

        targetLine = bestMatch;
    } else if (mode == EditMode::InsertBefore || mode == EditMode::InsertAfter) {
        if (lineNumber > 0 && lineNumber <= lines.size()) {
            targetLine = lineNumber - 1;
        }
    } else if (mode == EditMode::AppendToEnd) {
        if (!lines.isEmpty()) {
            int startLine = qMax(0, lines.size() - contextLines);
            contextBefore = lines.mid(startLine).join('\n');
        }
        return;
    }

    if (targetLine == -1) {
        return;
    }

    int startBefore = qMax(0, targetLine - contextLines);
    int countBefore = targetLine - startBefore;
    contextBefore = lines.mid(startBefore, countBefore).join('\n');

    int startAfter = targetLine + 1;
    int countAfter = qMin(contextLines, lines.size() - startAfter);
    contextAfter = lines.mid(startAfter, countAfter).join('\n');
}

} // namespace QodeAssist::Tools

