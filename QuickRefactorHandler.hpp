// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QObject>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>

#include <context/ContextManager.hpp>
#include <context/IDocumentReader.hpp>
#include <pluginllmcore/ContextData.hpp>
#include <pluginllmcore/Provider.hpp>

namespace QodeAssist {

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

    void sendRefactorRequest(TextEditor::TextEditorWidget *editor, const QString &instructions);

    void cancelRequest();
    bool isProcessing() const { return m_isRefactoringInProgress; }

signals:
    void refactoringCompleted(const QodeAssist::RefactorResult &result);

private slots:
    void handleFullResponse(const QString &requestId, const QString &fullText);
    void handleRequestFailed(const QString &requestId, const QString &error);

private:
    void prepareAndSendRequest(
        TextEditor::TextEditorWidget *editor,
        const QString &instructions,
        const Utils::Text::Range &range);

    void handleLLMResponse(const QString &response, const QJsonObject &request, bool isComplete);
    PluginLLMCore::ContextData prepareContext(
        TextEditor::TextEditorWidget *editor,
        const Utils::Text::Range &range,
        const QString &instructions);

    struct RequestContext
    {
        QJsonObject originalRequest;
        PluginLLMCore::Provider *provider;
    };

    QHash<QString, RequestContext> m_activeRequests;
    TextEditor::TextEditorWidget *m_currentEditor;
    Utils::Text::Range m_currentRange;
    bool m_isRefactoringInProgress;
    QString m_lastRequestId;
    Context::ContextManager m_contextManager;
};

} // namespace QodeAssist
