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

#include "LLMClientInterface.hpp"
#include "LSPCompletion.hpp"
#include "widgets/CompletionProgressHandler.hpp"
#include <languageclient/client.h>
#include <llmcore/IPromptProvider.hpp>
#include <llmcore/IProviderRegistry.hpp>

namespace QodeAssist {

class QodeAssistClient : public LanguageClient::Client
{
public:
    explicit QodeAssistClient(LLMClientInterface *clientInterface);
    ~QodeAssistClient() override;

    void openDocument(TextEditor::TextDocument *document) override;
    bool canOpenProject(ProjectExplorer::Project *project) override;

    void requestCompletions(TextEditor::TextEditorWidget *editor);

private:
    void scheduleRequest(TextEditor::TextEditorWidget *editor);
    void handleCompletions(
        const GetCompletionRequest::Response &response, TextEditor::TextEditorWidget *editor);
    void cancelRunningRequest(TextEditor::TextEditorWidget *editor);
    bool isEnabled(ProjectExplorer::Project *project) const;

    void setupConnections();
    void cleanupConnections();

    QHash<TextEditor::TextEditorWidget *, GetCompletionRequest> m_runningRequests;
    QHash<TextEditor::TextEditorWidget *, QTimer *> m_scheduledRequests;
    QMetaObject::Connection m_documentOpenedConnection;
    QMetaObject::Connection m_documentClosedConnection;

    QElapsedTimer m_typingTimer;
    int m_recentCharCount;
    CompletionProgressHandler m_progressHandler;
};

} // namespace QodeAssist
