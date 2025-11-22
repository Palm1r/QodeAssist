/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
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

#pragma once

#include <QObject>

#include "LLMClientInterface.hpp"
#include "LSPCompletion.hpp"
#include "QuickRefactorHandler.hpp"
#include "RefactorSuggestionHoverHandler.hpp"
#include "widgets/CompletionProgressHandler.hpp"
#include "widgets/CompletionErrorHandler.hpp"
#include "widgets/CompletionHintHandler.hpp"
#include "widgets/EditorChatButtonHandler.hpp"
#include "widgets/RefactorWidgetHandler.hpp"
#include <languageclient/client.h>
#include <llmcore/IPromptProvider.hpp>
#include <llmcore/IProviderRegistry.hpp>

namespace QodeAssist {

class QodeAssistClient : public LanguageClient::Client
{
    Q_OBJECT
public:
    explicit QodeAssistClient(LLMClientInterface *clientInterface);
    ~QodeAssistClient() override;

    void openDocument(TextEditor::TextDocument *document) override;
    bool canOpenProject(ProjectExplorer::Project *project) override;

    void requestCompletions(TextEditor::TextEditorWidget *editor);
    void requestQuickRefactor(
        TextEditor::TextEditorWidget *editor, const QString &instructions = QString());
    
    bool isHintVisible() const;
    void hideHintAndRequestCompletion(TextEditor::TextEditorWidget *editor);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void scheduleRequest(TextEditor::TextEditorWidget *editor);
    void handleCompletions(
        const GetCompletionRequest::Response &response, TextEditor::TextEditorWidget *editor);
    void cancelRunningRequest(TextEditor::TextEditorWidget *editor);
    bool isEnabled(ProjectExplorer::Project *project) const;

    void setupConnections();
    void cleanupConnections();
    void handleRefactoringResult(const RefactorResult &result);
    void displayRefactoringSuggestion(const RefactorResult &result);
    void displayRefactoringWidget(const RefactorResult &result);

    void handleAutoRequestTrigger(TextEditor::TextEditorWidget *widget, int charsAdded, bool isSpaceOrTab);
    void handleHintBasedTrigger(TextEditor::TextEditorWidget *widget, int charsAdded, bool isSpaceOrTab, QTextCursor &cursor);

    QHash<TextEditor::TextEditorWidget *, GetCompletionRequest> m_runningRequests;
    QHash<TextEditor::TextEditorWidget *, QTimer *> m_scheduledRequests;
    QMetaObject::Connection m_documentOpenedConnection;
    QMetaObject::Connection m_documentClosedConnection;

    QElapsedTimer m_typingTimer;
    int m_recentCharCount;
    QTimer m_hintHideTimer;
    CompletionProgressHandler m_progressHandler;
    CompletionErrorHandler m_errorHandler;
    CompletionHintHandler m_hintHandler;
    EditorChatButtonHandler m_chatButtonHandler;
    QuickRefactorHandler *m_refactorHandler{nullptr};
    RefactorSuggestionHoverHandler *m_refactorHoverHandler{nullptr};
    RefactorWidgetHandler *m_refactorWidgetHandler{nullptr};
    LLMClientInterface *m_llmClient;
};

} // namespace QodeAssist
