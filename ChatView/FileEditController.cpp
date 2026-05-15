// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FileEditController.hpp"

#include <QJsonDocument>
#include <QJsonObject>

#include <coreplugin/editormanager/editormanager.h>
#include <texteditor/texteditor.h>

#include "ChatModel.hpp"
#include "Logger.hpp"
#include "context/ChangesManager.h"

namespace QodeAssist::Chat {

FileEditController::FileEditController(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
{
    auto &changes = Context::ChangesManager::instance();
    connect(&changes, &Context::ChangesManager::fileEditAdded, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::ChangesManager::fileEditApplied, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::ChangesManager::fileEditRejected, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::ChangesManager::fileEditUndone, this, [this](const QString &) {
        updateStats();
    });
    connect(&changes, &Context::ChangesManager::fileEditArchived, this, [this](const QString &) {
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
    if (Context::ChangesManager::instance().applyFileEdit(editId)) {
        emit infoMessage(QString("File edit applied successfully"));
        updateFileEditStatus(editId, "applied");
    } else {
        auto edit = Context::ChangesManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to apply file edit")
                : QString("Failed to apply file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::rejectFileEdit(const QString &editId)
{
    LOG_MESSAGE(QString("Rejecting file edit: %1").arg(editId));
    if (Context::ChangesManager::instance().rejectFileEdit(editId)) {
        emit infoMessage(QString("File edit rejected"));
        updateFileEditStatus(editId, "rejected");
    } else {
        auto edit = Context::ChangesManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to reject file edit")
                : QString("Failed to reject file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::undoFileEdit(const QString &editId)
{
    LOG_MESSAGE(QString("Undoing file edit: %1").arg(editId));
    if (Context::ChangesManager::instance().undoFileEdit(editId)) {
        emit infoMessage(QString("File edit undone successfully"));
        updateFileEditStatus(editId, "rejected");
    } else {
        auto edit = Context::ChangesManager::instance().getFileEdit(editId);
        emit errorOccurred(
            edit.statusMessage.isEmpty()
                ? QString("Failed to undo file edit")
                : QString("Failed to undo file edit: %1").arg(edit.statusMessage));
    }
}

void FileEditController::openFileEditInEditor(const QString &editId)
{
    LOG_MESSAGE(QString("Opening file edit in editor: %1").arg(editId));

    auto edit = Context::ChangesManager::instance().getFileEdit(editId);
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

            if (edit.status == Context::ChangesManager::Applied && !edit.newContent.isEmpty()) {
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

void FileEditController::updateFileEditStatus(const QString &editId, const QString &status)
{
    auto messages = m_chatModel->getChatHistory();
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].role == Chat::ChatModel::FileEdit && messages[i].id == editId) {
            QString content = messages[i].content;

            const QString marker = "QODEASSIST_FILE_EDIT:";
            int markerPos = content.indexOf(marker);

            QString jsonStr = content;
            if (markerPos >= 0) {
                jsonStr = content.mid(markerPos + marker.length());
            }

            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                obj["status"] = status;

                auto edit = Context::ChangesManager::instance().getFileEdit(editId);
                if (!edit.statusMessage.isEmpty()) {
                    obj["status_message"] = edit.statusMessage;
                }

                QString updatedContent = marker
                                         + QString::fromUtf8(
                                             QJsonDocument(obj).toJson(QJsonDocument::Compact));
                m_chatModel->updateMessageContent(editId, updatedContent);
                LOG_MESSAGE(QString("Updated file edit status to: %1").arg(status));
            }
            break;
        }
    }

    updateStats();
}

void FileEditController::applyAllForCurrentMessage()
{
    if (m_currentRequestId.isEmpty()) {
        emit errorOccurred(QString("No active message with file edits"));
        return;
    }

    LOG_MESSAGE(QString("Applying all file edits for message: %1").arg(m_currentRequestId));

    QString errorMsg;
    bool success = Context::ChangesManager::instance()
                       .reapplyAllEditsForRequest(m_currentRequestId, &errorMsg);

    if (success) {
        emit infoMessage(QString("All file edits applied successfully"));
    } else {
        emit errorOccurred(
            errorMsg.isEmpty()
                ? QString("Failed to apply some file edits")
                : QString("Failed to apply some file edits:\n%1").arg(errorMsg));
    }

    auto edits = Context::ChangesManager::instance().getEditsForRequest(m_currentRequestId);
    for (const auto &edit : edits) {
        if (edit.status == Context::ChangesManager::Applied) {
            updateFileEditStatus(edit.editId, "applied");
        }
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
    bool success = Context::ChangesManager::instance()
                       .undoAllEditsForRequest(m_currentRequestId, &errorMsg);

    if (success) {
        emit infoMessage(QString("All file edits undone successfully"));
    } else {
        emit errorOccurred(
            errorMsg.isEmpty()
                ? QString("Failed to undo some file edits")
                : QString("Failed to undo some file edits:\n%1").arg(errorMsg));
    }

    auto edits = Context::ChangesManager::instance().getEditsForRequest(m_currentRequestId);
    for (const auto &edit : edits) {
        if (edit.status == Context::ChangesManager::Rejected) {
            updateFileEditStatus(edit.editId, "rejected");
        }
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

    auto edits = Context::ChangesManager::instance().getEditsForRequest(m_currentRequestId);

    int total = edits.size();
    int applied = 0;
    int pending = 0;
    int rejected = 0;

    for (const auto &edit : edits) {
        switch (edit.status) {
        case Context::ChangesManager::Applied:
            applied++;
            break;
        case Context::ChangesManager::Pending:
            pending++;
            break;
        case Context::ChangesManager::Rejected:
            rejected++;
            break;
        case Context::ChangesManager::Archived:
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
