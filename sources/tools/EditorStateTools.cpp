// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "EditorStateTools.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPromise>
#include <QTextCursor>

#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>
#include <texteditor/texteditor.h>
#include <texteditor/textdocument.h>

namespace QodeAssist::Tools {

namespace {

constexpr int maxUnsavedTextChars = 128 * 1024;
constexpr int maxTotalUnsavedChars = 1024 * 1024;
constexpr int maxSelectionChars = 64 * 1024;
constexpr int maxOpenEditors = 200;

QFuture<LLMQore::ToolResult> readyResult(LLMQore::ToolResult result)
{
    QPromise<LLMQore::ToolResult> promise;
    promise.start();
    promise.addResult(std::move(result));
    promise.finish();
    return promise.future();
}

QFuture<LLMQore::ToolResult> readyJson(const QJsonObject &payload)
{
    return readyResult(
        LLMQore::ToolResult::text(
            QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact))));
}

QString clampText(const QString &text, int limit)
{
    return text.size() <= limit ? text : text.first(limit) + QStringLiteral("\n[truncated]");
}

} // namespace

ListOpenEditorsTool::ListOpenEditorsTool(QObject *parent)
    : LLMQore::BaseTool(parent)
{}

QString ListOpenEditorsTool::id() const
{
    return QStringLiteral("list_open_editors");
}

QString ListOpenEditorsTool::displayName() const
{
    return tr("List Open Editors");
}

QString ListOpenEditorsTool::description() const
{
    return QStringLiteral(
        "List the files currently open in Qt Creator, marking which one is active and which have "
        "unsaved changes. Set include_unsaved_text to get the in-editor text of modified files, "
        "which differs from what is on disk. Read-only.");
}

QJsonObject ListOpenEditorsTool::parametersSchema() const
{
    return QJsonObject{
        {"type", "object"},
        {"properties",
         QJsonObject{
             {"include_unsaved_text",
              QJsonObject{
                  {"type", "boolean"},
                  {"description", "Include the editor text of files with unsaved changes."}}}}},
        {"required", QJsonArray{}}};
}

void ListOpenEditorsTool::setIgnorePredicate(std::function<bool(const QString &)> predicate)
{
    m_ignorePredicate = std::move(predicate);
}

QFuture<LLMQore::ToolResult> ListOpenEditorsTool::executeAsync(const QJsonObject &input)
{
    Q_ASSERT_X(
        thread() == qApp->thread(),
        "ListOpenEditorsTool",
        "Qt Creator's document model may only be read on the GUI thread");

    const bool includeUnsaved = input.value("include_unsaved_text").toBool(false);

    const Core::IDocument *currentDocument = Core::EditorManager::currentDocument();
    const QList<Core::IDocument *> documents = Core::DocumentModel::openedDocuments();

    QJsonArray editors;
    int skipped = 0;
    int unsavedBudget = maxTotalUnsavedChars;

    for (const Core::IDocument *document : documents) {
        const auto *textDocument = qobject_cast<const TextEditor::TextDocument *>(document);
        if (!textDocument)
            continue;

        const QString filePath = textDocument->filePath().toUrlishString();
        if (m_ignorePredicate && m_ignorePredicate(filePath)) {
            ++skipped;
            continue;
        }

        if (editors.size() >= maxOpenEditors) {
            ++skipped;
            continue;
        }

        QJsonObject entry{
            {"path", filePath},
            {"modified", textDocument->isModified()},
            {"active", document == currentDocument}};

        if (includeUnsaved && textDocument->isModified()) {
            if (unsavedBudget <= 0) {
                entry["unsaved_text_omitted"] = true;
            } else {
                const QString text
                    = clampText(textDocument->plainText(), qMin(unsavedBudget, maxUnsavedTextChars));
                unsavedBudget -= text.size();
                entry["unsaved_text"] = text;
            }
        }

        editors.append(entry);
    }

    QJsonObject payload{{"editors", editors}};
    if (skipped > 0)
        payload["omitted"] = skipped;

    return readyJson(payload);
}

GetEditorSelectionTool::GetEditorSelectionTool(QObject *parent)
    : LLMQore::BaseTool(parent)
{}

QString GetEditorSelectionTool::id() const
{
    return QStringLiteral("get_editor_selection");
}

QString GetEditorSelectionTool::displayName() const
{
    return tr("Get Editor Selection");
}

QString GetEditorSelectionTool::description() const
{
    return QStringLiteral(
        "Return what the user is currently looking at in Qt Creator: the active file, the cursor "
        "position, and the selected text if there is a selection. Read-only.");
}

QJsonObject GetEditorSelectionTool::parametersSchema() const
{
    return QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}, {"required", QJsonArray{}}};
}

QFuture<LLMQore::ToolResult> GetEditorSelectionTool::executeAsync(const QJsonObject &input)
{
    Q_UNUSED(input)

    auto *editor = TextEditor::BaseTextEditor::currentTextEditor();
    if (!editor)
        return readyJson(QJsonObject{{"active", false}});

    const QTextCursor cursor = editor->editorWidget()->textCursor();

    QJsonObject payload{
        {"active", true},
        {"path", editor->document()->filePath().toUrlishString()},
        {"line", cursor.blockNumber() + 1},
        {"column", cursor.positionInBlock() + 1},
        {"has_selection", cursor.hasSelection()}};

    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        selected.replace(QChar(0x2029), QLatin1Char('\n'));
        payload["selected_text"] = clampText(selected, maxSelectionChars);
        QTextCursor start = cursor;
        start.setPosition(cursor.selectionStart());
        QTextCursor end = cursor;
        end.setPosition(cursor.selectionEnd());
        payload["selection_start_line"] = start.blockNumber() + 1;
        payload["selection_end_line"] = end.blockNumber() + 1;
    }

    return readyJson(payload);
}

GetProjectModelTool::GetProjectModelTool(QObject *parent)
    : LLMQore::BaseTool(parent)
{}

QString GetProjectModelTool::id() const
{
    return QStringLiteral("get_project_model");
}

QString GetProjectModelTool::displayName() const
{
    return tr("Get Project Model");
}

QString GetProjectModelTool::description() const
{
    return QStringLiteral(
        "Return how Qt Creator has this project configured: project name and file, the active "
        "kit, build configuration and build directory. This is Qt Creator's own model, not "
        "something that can be read off disk. Read-only.");
}

QJsonObject GetProjectModelTool::parametersSchema() const
{
    return QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}, {"required", QJsonArray{}}};
}

QFuture<LLMQore::ToolResult> GetProjectModelTool::executeAsync(const QJsonObject &input)
{
    Q_UNUSED(input)

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project)
        return readyJson(QJsonObject{{"open", false}});

    QJsonObject payload{
        {"open", true},
        {"name", project->displayName()},
        {"project_file", project->projectFilePath().toUrlishString()},
        {"root", project->projectDirectory().toUrlishString()}};

    if (auto *target = project->activeTarget()) {
        if (auto *kit = target->kit())
            payload["kit"] = kit->displayName();

        if (auto *buildConfiguration = target->activeBuildConfiguration()) {
            payload["build_configuration"] = buildConfiguration->displayName();
            payload["build_directory"] = buildConfiguration->buildDirectory().toUrlishString();
        }
    }

    return readyJson(payload);
}

} // namespace QodeAssist::Tools
