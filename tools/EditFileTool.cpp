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

#include <context/ProjectUtils.hpp>
#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <settings/GeneralSettings.hpp>
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
    return "Edit a project file with two distinct modes of operation. "
           "IMPORTANT: All text fields must contain COMPLETE LINES with trailing newlines (\\n). "
           "If you forget to add \\n, it will be added automatically.\n"
           "\n"
           "TWO MODES OF OPERATION:\n"
           "\n"
           "1. REPLACE MODE (mode='replace'):\n"
           "   - Finds search_text in the file and replaces it with new_text\n"
           "   - REQUIRED: search_text (the text to find and replace)\n"
           "   - OPTIONAL: line_before and/or line_after for context verification\n"
           "   - Context verification strategy:\n"
           "     * line_before/line_after are searched NEAR search_text (within ~500 chars)\n"
           "     * They do NOT need to be immediately adjacent to search_text\n"
           "     * Both provided: BOTH must be found near search_text (most precise)\n"
           "     * Only line_before: must be found BEFORE search_text\n"
           "     * Only line_after: must be found AFTER search_text\n"
           "     * Neither: accepts first occurrence (use when search_text is unique)\n"
           "   - Use this to replace existing content\n"
           "\n"
           "2. INSERT_AFTER MODE (mode='insert_after'):\n"
           "   - Inserts new_text after the line specified in line_before\n"
           "   - If line_before is empty, inserts at the beginning of the file (useful for empty files)\n"
           "   - OPTIONAL: line_before (the line to insert after; empty = insert at start)\n"
           "   - OPTIONAL: line_after (must IMMEDIATELY follow line_before for verification)\n"
           "   - search_text is IGNORED\n"
           "   - Use this to insert content after a specific line or at the start of file\n"
           "   - Note: In this mode, new_text is inserted RIGHT AFTER line_before\n"
           "\n"
           "BEST PRACTICES for multiple edits to the same file:\n"
           "- Use INSERT_AFTER mode for sequential additions (each edit uses previous addition as line_before)\n"
           "- Provide stable context lines that won't be modified by current or subsequent edits\n"
           "- For empty files: use INSERT_AFTER with empty line_before\n"
           "- Example: When adding multiple class properties:\n"
           "  * First edit: INSERT_AFTER with class declaration as line_before\n"
           "  * Second edit: INSERT_AFTER with first property as line_before\n"
           "  * Third edit: INSERT_AFTER with second property as line_before\n"
           "\n"
           "Parameters:\n"
           "- mode: 'replace' or 'insert_after'\n"
           "- filepath: absolute or relative file path to edit\n"
           "- new_text: complete line(s) to insert/replace (with \\n)\n"
           "- search_text: (replace mode only) complete line(s) to find and replace (with \\n)\n"
           "- line_before: complete line for context (with \\n), usage depends on mode\n"
           "- line_after: complete line for context (with \\n), usage depends on mode";
}

QJsonObject EditFileTool::getDefinition(LLMCore::ToolSchemaFormat format) const
{
    QJsonObject properties;

    QJsonObject modeProperty;
    modeProperty["type"] = "string";
    modeProperty["enum"] = QJsonArray({"replace", "insert_after"});
    modeProperty["description"] = "Edit mode: 'replace' (replace search_text with new_text), "
                                   "'insert_after' (insert new_text after line_before)";
    properties["mode"] = modeProperty;

    QJsonObject filepathProperty;
    filepathProperty["type"] = "string";
    filepathProperty["description"] = "The absolute or relative file path to edit";
    properties["filepath"] = filepathProperty;

    QJsonObject newTextProperty;
    newTextProperty["type"] = "string";
    newTextProperty["description"] = "Complete line(s) to insert/replace/append. Trailing newline (\\n) auto-added if missing. "
                                      "Example: 'int main(int argc, char *argv[]) {\\n' or 'void foo();\\nvoid bar();\\n'";
    properties["new_text"] = newTextProperty;

    QJsonObject searchTextProperty;
    searchTextProperty["type"] = "string";
    searchTextProperty["description"]
        = "Complete line(s) to search for and replace. Trailing newline (\\n) auto-added if missing. "
          "REQUIRED for 'replace' mode, IGNORED for other modes. "
          "Example: 'int main() {\\n' or 'void foo();\\n'";
    properties["search_text"] = searchTextProperty;

    QJsonObject lineBeforeProperty;
    lineBeforeProperty["type"] = "string";
    lineBeforeProperty["description"] = "Complete line for context verification. Trailing newline (\\n) auto-added if missing. "
                                         "Usage depends on mode:\n"
                                         "- 'replace': OPTIONAL, searched BEFORE search_text (within ~500 chars, not necessarily adjacent)\n"
                                         "- 'insert_after': OPTIONAL, new_text inserted RIGHT AFTER this line. "
                                         "If empty, inserts at the beginning of the file (useful for empty files)\n"
                                         "Example: 'class Movie {\\n' or '#include <iostream>\\n'";
    properties["line_before"] = lineBeforeProperty;

    QJsonObject lineAfterProperty;
    lineAfterProperty["type"] = "string";
    lineAfterProperty["description"] = "Complete line for context verification. Trailing newline (\\n) auto-added if missing. "
                                        "Usage depends on mode:\n"
                                        "- 'replace': OPTIONAL, searched AFTER search_text (within ~500 chars, not necessarily adjacent)\n"
                                        "- 'insert_after': OPTIONAL, must IMMEDIATELY follow line_before for verification\n"
                                        "Example: '}\\n' or 'public:\\n'";
    properties["line_after"] = lineAfterProperty;

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;

    QJsonArray required;
    required.append("mode");
    required.append("filepath");
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

LLMCore::ToolPermissions EditFileTool::requiredPermissions() const
{
    return LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemRead) 
         | LLMCore::ToolPermissions(LLMCore::ToolPermission::FileSystemWrite);
}

QFuture<QString> EditFileTool::executeAsync(const QJsonObject &input)
{
    return QtConcurrent::run([this, input]() -> QString {
        QString mode = input["mode"].toString();
        if (mode.isEmpty()) {
            throw ToolInvalidArgument("Error: mode parameter is required. Must be one of: 'replace', 'insert_after'");
        }

        if (mode != "replace" && mode != "insert_after") {
            throw ToolInvalidArgument(QString("Error: invalid mode '%1'. Must be one of: 'replace', 'insert_after'").arg(mode));
        }

        QString inputFilepath = input["filepath"].toString();
        if (inputFilepath.isEmpty()) {
            throw ToolInvalidArgument("Error: filepath parameter is required");
        }

        QString newText = input["new_text"].toString();
        if (newText.isEmpty()) {
            throw ToolInvalidArgument("Error: new_text parameter is required");
        }

        QString searchText = input["search_text"].toString();
        QString lineBefore = input["line_before"].toString();
        QString lineAfter = input["line_after"].toString();

        if (mode == "replace" && searchText.isEmpty()) {
            throw ToolInvalidArgument("Error: search_text is required for 'replace' mode");
        }

        // Normalize text fields: ensure trailing newline if not empty
        // This handles cases where LLM forgets to add \n
        auto normalizeText = [](QString &text) {
            if (!text.isEmpty() && !text.endsWith('\n')) {
                LOG_MESSAGE(QString("EditFileTool: normalizing text, adding trailing newline (length: %1)").arg(text.length()));
                text += '\n';
            }
        };

        normalizeText(newText);
        if (!searchText.isEmpty()) normalizeText(searchText);
        if (!lineBefore.isEmpty()) normalizeText(lineBefore);
        if (!lineAfter.isEmpty()) normalizeText(lineAfter);

        QString filePath;
        QFileInfo fileInfo(inputFilepath);

        if (fileInfo.isAbsolute()) {
            filePath = inputFilepath;
        } else {
            auto projects = ProjectExplorer::ProjectManager::projects();
            if (!projects.isEmpty() && projects.first()) {
                QString projectDir = projects.first()->projectDirectory().toUrlishString();
                filePath = QDir(projectDir).absoluteFilePath(inputFilepath);
            } else {
                filePath = QFileInfo(inputFilepath).absoluteFilePath();
            }
        }

        if (!QFileInfo::exists(filePath)) {
            throw ToolRuntimeError(QString("Error: File '%1' does not exist").arg(filePath));
        }

        bool isInProject = Context::ProjectUtils::isFileInProject(filePath);
        
        if (!isInProject) {
            const auto &settings = Settings::generalSettings();
            if (!settings.allowAccessOutsideProject()) {
                throw ToolRuntimeError(
                    QString("Error: File '%1' is outside the project scope. "
                            "Enable 'Allow file access outside project' in settings to edit files outside project scope.")
                        .arg(filePath));
            }
            LOG_MESSAGE(QString("Editing file outside project scope: %1").arg(filePath));
        }

        auto project = isInProject ? ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath)) : nullptr;
        
        if (project && m_ignoreManager->shouldIgnore(filePath, project)) {
            throw ToolRuntimeError(
                QString("Error: File '%1' is excluded by .qodeassistignore").arg(inputFilepath));
        }

        QJsonObject result;
        result["type"] = "file_edit";
        result["mode"] = mode;
        result["filepath"] = filePath;
        result["new_text"] = newText;
        result["search_text"] = searchText;
        result["line_before"] = lineBefore;
        result["line_after"] = lineAfter;

        QJsonDocument doc(result);
        return QString("QODEASSIST_FILE_EDIT:%1")
            .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    });
}

} // namespace QodeAssist::Tools

