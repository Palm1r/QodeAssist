// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <LLMQore/BaseClient.hpp>
#include <texteditor/texteditor.h>
#include <utils/textutils.h>

#include <ErrorInfo.hpp>
#include <context/ContextManager.hpp>
#include <context/IDocumentReader.hpp>

namespace QodeAssist {

class SessionManager;
class Session;
class AgentFactory;

struct RefactorResult
{
    QString newText;
    Utils::Text::Range insertRange;
    bool success;
    QString errorMessage;
    TextEditor::TextEditorWidget *editor{nullptr};
};

class QuickRefactorHandler : public QObject
{
    Q_OBJECT

public:
    explicit QuickRefactorHandler(QObject *parent = nullptr);
    ~QuickRefactorHandler() override;

    void setSessionManager(SessionManager *sessionManager);
    void setAgentFactory(AgentFactory *agentFactory);

    void sendRefactorRequest(TextEditor::TextEditorWidget *editor, const QString &instructions);

    void cancelRequest();
    bool isProcessing() const { return m_isRefactoringInProgress; }

    static QString configuredAgent(AgentFactory *agentFactory);

signals:
    void refactoringCompleted(const QodeAssist::RefactorResult &result);

private:
    void prepareAndSendRequest(
        TextEditor::TextEditorWidget *editor,
        const QString &instructions,
        const Utils::Text::Range &range);

    void onRefactorFinished(const QString &requestId);
    void onRefactorFailed(const QString &requestId, const QodeAssist::ErrorInfo &error);
    QString buildContextLayer(
        TextEditor::TextEditorWidget *editor, const Utils::Text::Range &range);
    QString pickRefactorAgent() const;

    struct RequestContext
    {
        QJsonObject originalRequest;
        QPointer<Session> session;
    };

    QPointer<SessionManager> m_sessionManager;
    QPointer<AgentFactory> m_agentFactory;
    QHash<QString, RequestContext> m_activeRequests;
    TextEditor::TextEditorWidget *m_currentEditor;
    Utils::Text::Range m_currentRange;
    bool m_isRefactoringInProgress;
    QString m_lastRequestId;
    Context::ContextManager m_contextManager;
};

} // namespace QodeAssist
