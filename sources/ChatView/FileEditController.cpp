// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "FileEditController.hpp"

#include <QJsonDocument>
#include <QJsonObject>

#include <coreplugin/editormanager/editormanager.h>
#include <texteditor/texteditor.h>

#include "Logger.hpp"
#include "context/FileEditManager.hpp"

namespace QodeAssist::Chat {

FileEditController::FileEditController(QObject *parent)
    : QObject(parent)
{
    auto &changes = Context::FileEditManager::instance();
    connect(&changes, &Context::FileEditManager::fileEditAdded, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::FileEditManager::fileEditApplied, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::FileEditManager::fileEditRejected, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::FileEditManager::fileEditUndone, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::FileEditManager::fileEditArchived, this, [this](const QString &) {
        updateStats();
    });
}

void FileEditController::setCurrentRequestId(const QString &requestId)
{
    if (!m_currentRequestId.isEmpty()) {
        LOG_MESSAGE(QString("Clearing previous message requestId: %1").arg(m_currentRequestId));
    }

    m_currentRequestId = requestId;
    LOG_MESSAGE(QString("New message request started: %1").arg(requestId));
    updateStats();
}

void FileEditController::clearCurrentRequestId()
{
    m_currentRequestId.clear();
    updateStats();
}

int FileEditController::totalEdits() const
{
    return m_totalEdits;
}

int FileEditController::appliedEdits() const
{
    return m_appliedEdits;
}

int FileEditController::pendingEdits() const
{
    return m_pendingEdits;
}

int FileEditController::rejectedEdits() const
{
    return m_rejectedEdits;
}

void FileEditController::applyFileEdit(const QString &editId)
{
    LOG_MESSAGE(QString("Applying file edit: %1").arg(editId));
    if (Context::FileEditManager::instance().applyFileEdit(editId)) {
        emit infoMessage(QString("File edit applied successfully"));
        updateStats();
    } else {
        auto edit = Context::FileEditManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to apply file edit")
                : QString("Failed to apply file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::rejectFileEdit(const QString &editId)
{
    LOG_MESSAGE(QString("Rejecting file edit: %1").arg(editId));
    if (Context::FileEditManager::instance().rejectFileEdit(editId)) {
        emit infoMessage(QString("File edit rejected"));
        updateStats();
    } else {
        auto edit = Context::FileEditManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to reject file edit")
                : QString("Failed to reject file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::undoFileEdit(const QString &editId)
{
    LOG_MESSAGE(QString("Undoing file edit: %1").arg(editId));
    if (Context::FileEditManager::instance().undoFileEdit(editId)) {
        emit infoMessage(QString("File edit undone successfully"));
        updateStats();
    } else {
        auto edit = Context::FileEditManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to undo file edit")
                : QString("Failed to undo file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::openFileEditInEditor(const QString &editId)
{
    LOG_MESSAGE(QString("Opening file edit in editor: %1").arg(editId));

    auto edit = Context::FileEditManager::instance().getFileEdit(editId);
    if (edit.editId.isEmpty()) {
        emit errorOccurred(QString("File edit not found: %1").arg(editId));
        return;
    }

    Utils::FilePath filePath = Utils::FilePath::fromString(edit.filePath);

    Core::IEditor *editor = Core::EditorManager::openEditor(filePath);
    if (!editor) {
        emit errorOccurred(QString("Failed to open file in editor: %1").arg(edit.filePath));
        return;
    }

    auto *textEditor = qobject_cast<TextEditor::BaseTextEditor *>(editor);
    if (textEditor && textEditor->editorWidget()) {
        QTextDocument *doc = textEditor->editorWidget()->document();
        if (doc) {
            QString currentContent = doc->toPlainText();
            int position = -1;

            if (edit.status == Context::FileEditManager::Applied && !edit.newContent.isEmpty()) {
                position = currentContent.indexOf(edit.newContent);
            } else if (!edit.oldContent.isEmpty()) {
                position = currentContent.indexOf(edit.oldContent);
            }

            if (position >= 0) {
                QTextCursor cursor(doc);
                cursor.setPosition(position);
                textEditor->editorWidget()->setTextCursor(cursor);
                textEditor->editorWidget()->centerCursor();
            }
        }
    }

    LOG_MESSAGE(QString("Opened file in editor: %1").arg(edit.filePath));
}

void FileEditController::applyAllForCurrentMessage()
{
    if (m_currentRequestId.isEmpty()) {
        emit errorOccurred(QString("No active message with file edits"));
        return;
    }

    LOG_MESSAGE(QString("Applying all file edits for message: %1").arg(m_currentRequestId));

    QString errorMsg;
    bool success = Context::FileEditManager::instance()
                       .reapplyAllEditsForRequest(m_currentRequestId, &errorMsg);

    if (success) {
        emit infoMessage(QString("All file edits applied successfully"));
    } else {
        emit errorOccurred(
            errorMsg.isEmpty()
                ? QString("Failed to apply some file edits")
                : QString("Failed to apply some file edits:\n%1").arg(errorMsg));
    }

    updateStats();
}

void FileEditController::undoAllForCurrentMessage()
{
    if (m_currentRequestId.isEmpty()) {
        emit errorOccurred(QString("No active message with file edits"));
        return;
    }

    LOG_MESSAGE(QString("Undoing all file edits for message: %1").arg(m_currentRequestId));

    QString errorMsg;
    bool success = Context::FileEditManager::instance()
                       .undoAllEditsForRequest(m_currentRequestId, &errorMsg);

    if (success) {
        emit infoMessage(QString("All file edits undone successfully"));
    } else {
        emit errorOccurred(
            errorMsg.isEmpty()
                ? QString("Failed to undo some file edits")
                : QString("Failed to undo some file edits:\n%1").arg(errorMsg));
    }

    updateStats();
}

void FileEditController::updateStats()
{
    if (m_currentRequestId.isEmpty()) {
        if (m_totalEdits != 0 || m_appliedEdits != 0 || m_pendingEdits != 0
            || m_rejectedEdits != 0) {
            m_totalEdits = 0;
            m_appliedEdits = 0;
            m_pendingEdits = 0;
            m_rejectedEdits = 0;
            emit statsChanged();
        }
        return;
    }

    auto edits = Context::FileEditManager::instance().getEditsForRequest(m_currentRequestId);

    int total = edits.size();
    int applied = 0;
    int pending = 0;
    int rejected = 0;

    for (const auto &edit : edits) {
        switch (edit.status) {
        case Context::FileEditManager::Applied:
            applied++;
            break;
        case Context::FileEditManager::Pending:
            pending++;
            break;
        case Context::FileEditManager::Rejected:
            rejected++;
            break;
        case Context::FileEditManager::Archived:
            total--;
            break;
        }
    }

    bool changed = false;
    if (m_totalEdits != total) {
        m_totalEdits = total;
        changed = true;
    }
    if (m_appliedEdits != applied) {
        m_appliedEdits = applied;
        changed = true;
    }
    if (m_pendingEdits != pending) {
        m_pendingEdits = pending;
        changed = true;
    }
    if (m_rejectedEdits != rejected) {
        m_rejectedEdits = rejected;
        changed = true;
    }

    if (changed) {
        LOG_MESSAGE(
            QString("Updated message edits stats: total=%1, applied=%2, pending=%3, rejected=%4")
                .arg(total)
                .arg(applied)
                .arg(pending)
                .arg(rejected));
        emit statsChanged();
    }
}

} // namespace QodeAssist::Chat
