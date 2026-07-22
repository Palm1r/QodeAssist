// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include "completion/CompletionEngine.hpp"
#include "context/ContextManager.hpp"
#include "refactor/QuickRefactorHandler.hpp"
#include "refactor/RefactorSuggestionHoverHandler.hpp"
#include "widgets/CompletionErrorHandler.hpp"
#include "widgets/CompletionProgressHandler.hpp"
#include "widgets/RefactorWidgetHandler.hpp"

namespace QodeAssist {

class CodeCompletionController : public QObject
{
    Q_OBJECT

public:
    CodeCompletionController(
        CompletionEngine *fimEngine,
        CompletionEngine *agenticEngine,
        CompletionEngine *acpEngine,
        Context::ContextManager &contextManager,
        QObject *parent = nullptr);
    ~CodeCompletionController() override;

    void requestCompletions(TextEditor::TextEditorWidget *editor);
    void requestQuickRefactor(
        TextEditor::TextEditorWidget *editor, const QString &instructions = QString());

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct RunningRequest
    {
        quint64 id = 0;
        int position = -1;
        CompletionEngine *engine = nullptr;
    };

    void openDocument(TextEditor::TextDocument *document);
    void closeDocument(TextEditor::TextDocument *document);
    void scheduleRequest(TextEditor::TextEditorWidget *editor);
    void handleCompletionReady(quint64 requestId, const QString &text);
    void handleCompletionFailed(quint64 requestId, const QString &error);
    void applyCompletion(
        TextEditor::TextEditorWidget *editor, const QString &text, int requestPosition);
    void cancelRunningRequest(TextEditor::TextEditorWidget *editor);
    void dropRunningRequestById(quint64 requestId);
    bool isEnabled(ProjectExplorer::Project *project) const;
    CompletionEngine *activeEngine() const;

    void setupConnections();
    void cleanupConnections();
    void handleRefactoringResult(const RefactorResult &result);
    void displayRefactoringSuggestion(const RefactorResult &result);
    void displayRefactoringWidget(const RefactorResult &result);
    void applyRefactoringEdit(
        TextEditor::TextEditorWidget *editor, const Utils::Text::Range &range, const QString &text);

    void handleAutoRequestTrigger(TextEditor::TextEditorWidget *widget);

    CompletionEngine *m_fimEngine = nullptr;
    CompletionEngine *m_agenticEngine = nullptr;
    CompletionEngine *m_acpEngine = nullptr;
    Context::ContextManager &m_contextManager;
    quint64 m_lastRequestId = 0;
    QHash<TextEditor::TextEditorWidget *, RunningRequest> m_runningRequests;
    QHash<quint64, QPointer<TextEditor::TextEditorWidget>> m_requestEditors;
    QHash<TextEditor::TextEditorWidget *, QTimer *> m_scheduledRequests;
    QHash<TextEditor::TextDocument *, QMetaObject::Connection> m_documentConnections;
    QList<QPointer<TextEditor::TextEditorWidget>> m_hoverHandlerWidgets;
    QMetaObject::Connection m_documentOpenedConnection;
    QMetaObject::Connection m_documentClosedConnection;

    QElapsedTimer m_typingTimer;
    int m_recentCharCount;
    CompletionProgressHandler m_progressHandler;
    CompletionErrorHandler m_errorHandler;
    QuickRefactorHandler *m_refactorHandler{nullptr};
    RefactorSuggestionHoverHandler *m_refactorHoverHandler{nullptr};
    RefactorWidgetHandler *m_refactorWidgetHandler{nullptr};
};

} // namespace QodeAssist
