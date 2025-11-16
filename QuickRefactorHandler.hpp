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

#pragma once

#include <QJsonObject>
#include <QObject>

#include <texteditor/texteditor.h>
#include <utils/textutils.h>

#include <context/ContextManager.hpp>
#include <context/IDocumentReader.hpp>
#include <llmcore/ContextData.hpp>
#include <llmcore/Provider.hpp>

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
    LLMCore::ContextData prepareContext(
        TextEditor::TextEditorWidget *editor,
        const Utils::Text::Range &range,
        const QString &instructions);

    struct RequestContext
    {
        QJsonObject originalRequest;
        LLMCore::Provider *provider;
    };

    QHash<QString, RequestContext> m_activeRequests;
    TextEditor::TextEditorWidget *m_currentEditor;
    Utils::Text::Range m_currentRange;
    bool m_isRefactoringInProgress;
    QString m_lastRequestId;
    Context::ContextManager m_contextManager;
};

} // namespace QodeAssist
