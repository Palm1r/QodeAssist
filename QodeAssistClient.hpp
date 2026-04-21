// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <pluginllmcore/IPromptProvider.hpp>
#include <pluginllmcore/IProviderRegistry.hpp>

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
    void applyRefactoringEdit(TextEditor::TextEditorWidget *editor, const Utils::Text::Range &range, const QString &text);

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
